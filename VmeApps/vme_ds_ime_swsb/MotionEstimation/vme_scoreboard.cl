/*****************************************************************************\
  Dependency bit settings
\*****************************************************************************/

#define USE_OPENCL_2_0_ATOMICS (1)

#define TOP_DEP         (1 << 0)
#define TOP_LEFT_DEP    (1 << 1)
#define LEFT_DEP        (1 << 2)
#define ALL_DEP         (TOP_DEP | TOP_LEFT_DEP | LEFT_DEP)

#if USE_OPENCL_2_0_ATOMICS
typedef atomic_int ATOMIC_INT;
#else
typedef int ATOMIC_INT;
#endif

/*****************************************************************************\
Kernel Function:
    initialize_scoreboard

Description:
    Initializes the scoreboard, considering the frame boundaries. For the TOP,
    LEFT and TOP_LEFT boundaries, the macroblock dependencies are cleared
    appropriately.
\*****************************************************************************/

__kernel
void initialize_scoreboard(__global uint* scoreboard, int num_mb_x) 
{
    int x = get_global_id(0);
    int y = get_global_id(1);

    if( x < num_mb_x)
    {
        int lid = y * num_mb_x + x;
        uint initValue = 0;        
        
        if(y == 0) initValue |= TOP_DEP;
        if(x == 0) initValue |= LEFT_DEP;
        if((x == 0) || (y == 0)) initValue |= TOP_LEFT_DEP;

        scoreboard[lid] = initValue;
    }
}

/*****************************************************************************\
Function:
    poll_scoreboard

Description:
    Poll the scoreboard entry for mbid until all its dependencies are cleared.
\*****************************************************************************/
void poll_scoreboard(int2 mbid, int2 ngrp, __global  ATOMIC_INT* scoreboard)
{
    int s = 0;
    while (s != ALL_DEP)
    {
#if USE_OPENCL_2_0_ATOMICS
      s = 
		atomic_load_explicit( 
			scoreboard + mbid.y * ngrp.x + mbid.x, 
			memory_order_acquire, 
			memory_scope_device);
#else
	  s = scoreboard[mbid.y * ngrp.x + mbid.x];       
#endif
    }
}

/*****************************************************************************\
Function:
    signal_scoreboard

Description:
    Clear mbid's dependency in the scoreboard entries of its depending MB.
\*****************************************************************************/
void signal_scoreboard(int2 mbid, int2 ngrp,__global  ATOMIC_INT* scoreboard)
{
#if USE_OPENCL_2_0_ATOMICS
    if (mbid.y < ngrp.y - 1) {		
        atomic_fetch_or_explicit(
			scoreboard + (mbid.y+1) * ngrp.x + mbid.x, 
			TOP_DEP, 
			memory_order_acq_rel, 
			memory_scope_device );
	}
    if (mbid.x < ngrp.x - 1) {		
        atomic_fetch_or_explicit(
			scoreboard + mbid.y * ngrp.x + mbid.x + 1, 
			LEFT_DEP, 
			memory_order_acq_rel,
			memory_scope_device );
	}
    if ((mbid.x < ngrp.x - 1) && (mbid.y < ngrp.y - 1)) {		
        atomic_fetch_or_explicit(
			scoreboard + (mbid.y + 1) * ngrp.x + mbid.x + 1, 
			TOP_LEFT_DEP, 
			memory_order_acq_rel, 
			memory_scope_device );
	}
#else
	if (mbid.y < ngrp.y - 1)
        atomic_or(&scoreboard[(mbid.y+1) * ngrp.x + mbid.x], TOP_DEP);
    if (mbid.x < ngrp.x - 1)
        atomic_or(&scoreboard[mbid.y * ngrp.x + mbid.x + 1], LEFT_DEP);
    if ((mbid.x < ngrp.x - 1) && (mbid.y < ngrp.y - 1))
        atomic_or(&scoreboard[(mbid.y + 1) * ngrp.x + mbid.x + 1], TOP_LEFT_DEP);
#endif
}

/*****************************************************************************\
Function:
    get_cost_center

Description:
    Get the cost center corresponding to the input integer offset.
\*****************************************************************************/
ulong get_cost_center(short value) 
{
    short2 costCoord;
    // Cost coords are in QPEL resolution.
    costCoord.s0 = value << 2;
    costCoord.s1 = value << 2;
    uint2 cost_center_int;
    cost_center_int.s0 = as_uint( costCoord );
    cost_center_int.s1 = 0;
    return as_ulong( cost_center_int );
    }

/*****************************************************************************\
Kernel Function:
    block_motion_estimate_intel

Description:
    Performs block motion estimation with single reference frame, and computes
    the motion vectors, distortions, major and minor shapes by invoking VME 
    builtin functions. Performs the execution of individual macro blocks in
    the order defined by the software score-board and using the output MVs
    from the neighboring blocks as predictors for the current block and taking
    the best across them and using a zero cost center.
\*****************************************************************************/

__kernel __attribute__((reqd_work_group_size(16,1,1)))
void block_motion_estimate_intel(
    __read_only image2d_t   srcImg,
    __read_only image2d_t   refImg,
    __global short2*        prediction_motion_vector_buffer,
    __global short2*        motion_vector_buffer,
    __global ushort*        residuals_buffer,
    __global uchar2*        shapes_buffer,
    __global ATOMIC_INT*    scoreboard,
    __global short2*        launch_buffer ) 
{
    int2 gid = { get_group_id(0), get_group_id(1) };
    int2 ngrp = { get_num_groups(0), get_num_groups(1) };   
    
    int2 mbid = gid;
    int lid = gid.y * ngrp.x + gid.x;       
    mbid.x = launch_buffer[lid].x;
    mbid.y = launch_buffer[lid].y;
    
    // ....................Blocking phase.................................
	
    if( get_sub_group_local_id() == 0 )
    {
        poll_scoreboard( mbid, ngrp, scoreboard);
    }

    // ....................Processing phase.................................

    sampler_t accelerator       = CLK_AVC_ME_INITIALIZE_INTEL;
    uchar partition_mask        = CLK_AVC_ME_PARTITION_MASK_ALL_INTEL;
    uchar sad_adjustment        = CLK_AVC_ME_SAD_ADJUST_MODE_NONE_INTEL;
    uchar pixel_mode            = CLK_AVC_ME_SUBPIXEL_MODE_QPEL_INTEL;
    uchar search_cost_precision = CLK_AVC_ME_COST_PRECISION_QPEL_INTEL; 

    short2 predMV0, predMV1, predMV2, predMV3;
    
    if (mbid.x == 0 && mbid.y == 0) {
        predMV0 = predMV1 = predMV2 = predMV3 = 0;
    }
    else if (mbid.x == 0) { 
        predMV1 = predMV2 = motion_vector_buffer [ mbid.x * 16 + (mbid.y - 1) * 16 * ngrp.x ];
        predMV0 = predMV3 = 0;
    }
    else if (mbid.y == 0) { 
        predMV1 = predMV3 = motion_vector_buffer [ (mbid.x - 1) * 16 + mbid.y * 16 * ngrp.x ];
        predMV0 = predMV2 = 0;
    }
    else {
        predMV0 = 0;
        predMV1 = motion_vector_buffer [ (mbid.x - 1) * 16 + (mbid.y - 1) * 16 * ngrp.x ];
        predMV2 = motion_vector_buffer [ mbid.x * 16 + (mbid.y - 1) * 16 * ngrp.x ];
        predMV3 = motion_vector_buffer [ (mbid.x - 1) * 16 + mbid.y * 16 * ngrp.x ];
    }

    ushort2 srcCoord;
    short2 refCoord0, refCoord1, refCoord2, refCoord3;
    ulong cost_center0, cost_center1, cost_center2, cost_center3;

    srcCoord.x = mbid.x * 16 + get_global_offset(0);
    srcCoord.y = mbid.y * 16 + get_global_offset(1);

    // Obtain refCoord in PEL resolution and cost center in QPEL resolution.
    refCoord0 = predMV0 >> 2;
    refCoord1 = predMV1 >> 2;
    refCoord2 = predMV2 >> 2;
    refCoord3 = predMV3 >> 2;
    
    // Use zero cost centers to get shorter MVs.
    cost_center0 = get_cost_center(0);
    cost_center1 = get_cost_center(0);
    cost_center2 = get_cost_center(0);
    cost_center3 = get_cost_center(0);
    
    // Ensure that the reference search window is atleast partially within the reference frame.

    ushort2 framesize = convert_ushort2(get_image_dim(srcImg));
    ushort2 searchSize = intel_sub_group_ime_ref_window_size(CLK_AVC_ME_SEARCH_WINDOW_EXHAUSTIVE_INTEL, 0);
    refCoord0 = intel_sub_group_avc_ime_adjust_ref_offset(refCoord0, srcCoord, searchSize, framesize);
    refCoord1 = intel_sub_group_avc_ime_adjust_ref_offset(refCoord1, srcCoord, searchSize, framesize);
    refCoord2 = intel_sub_group_avc_ime_adjust_ref_offset(refCoord2, srcCoord, searchSize, framesize);
    refCoord3 = intel_sub_group_avc_ime_adjust_ref_offset(refCoord3, srcCoord, searchSize, framesize);

    uint2 packed_cost_table = intel_sub_group_avc_mce_get_default_high_penalty_cost_table();

    // IME with 1st predictor. Streamout results for taking the best with next IME.

    short2 refCoord = refCoord0;
    ulong cost_center = cost_center0;
    intel_sub_group_avc_ime_payload_t payload = intel_sub_group_avc_ime_initialize( srcCoord, partition_mask, sad_adjustment );
    payload = intel_sub_group_avc_ime_set_single_reference( refCoord, CLK_AVC_ME_SEARCH_WINDOW_EXHAUSTIVE_INTEL, payload );
    payload = intel_sub_group_avc_ime_set_motion_vector_cost_function( cost_center, packed_cost_table, search_cost_precision, payload );    
    intel_sub_group_avc_ime_result_single_reference_streamout_t resultsout;
    resultsout = intel_sub_group_avc_ime_evaluate_with_single_reference_streamout( srcImg, refImg, accelerator, payload );

    // IME with 2nd predictor and streamin results with previous IME. Streamout results for taking the best with next IME.

    refCoord = refCoord1;
    cost_center = cost_center1;
    intel_sub_group_avc_ime_single_reference_streamin_t resultsin = intel_sub_group_avc_ime_get_single_reference_streamin( resultsout );
    payload = intel_sub_group_avc_ime_initialize( srcCoord, partition_mask, sad_adjustment );
    payload = intel_sub_group_avc_ime_set_single_reference( refCoord, CLK_AVC_ME_SEARCH_WINDOW_EXHAUSTIVE_INTEL, payload );
    payload = intel_sub_group_avc_ime_set_motion_vector_cost_function( cost_center, packed_cost_table, search_cost_precision, payload );    
    resultsout = intel_sub_group_avc_ime_evaluate_with_single_reference_streaminout( srcImg, refImg, accelerator, payload, resultsin );
    resultsin = intel_sub_group_avc_ime_get_single_reference_streamin( resultsout );

    // IME with 3rd predictor and streamin results with previous IME. Streamout results for taking the best with next IME.

    refCoord = refCoord2;
    cost_center = cost_center2;
    resultsin = intel_sub_group_avc_ime_get_single_reference_streamin( resultsout );
    payload = intel_sub_group_avc_ime_initialize( srcCoord, partition_mask, sad_adjustment );
    payload = intel_sub_group_avc_ime_set_single_reference( refCoord, CLK_AVC_ME_SEARCH_WINDOW_EXHAUSTIVE_INTEL, payload );
    payload = intel_sub_group_avc_ime_set_motion_vector_cost_function( cost_center, packed_cost_table, search_cost_precision, payload ) ;    
    resultsout = intel_sub_group_avc_ime_evaluate_with_single_reference_streaminout( srcImg, refImg, accelerator, payload, resultsin );

    // IME with 4th predictor and streamin results with previous IME. 

    refCoord = refCoord3;
    cost_center = cost_center3;
    resultsin = intel_sub_group_avc_ime_get_single_reference_streamin( resultsout );
    payload = intel_sub_group_avc_ime_initialize( srcCoord, partition_mask, sad_adjustment );
    payload = intel_sub_group_avc_ime_set_motion_vector_cost_function( cost_center, packed_cost_table, search_cost_precision, payload );
    payload = intel_sub_group_avc_ime_set_single_reference( refCoord, CLK_AVC_ME_SEARCH_WINDOW_EXHAUSTIVE_INTEL, payload );
    intel_sub_group_avc_ime_result_t result = intel_sub_group_avc_ime_evaluate_with_single_reference_streamin( srcImg, refImg, accelerator, payload, resultsin );

    // Get the best MVs, distortions and shapes across all previous IMEs.

    long mvs           = intel_sub_group_avc_ime_get_motion_vectors( result );
    ushort dists       = intel_sub_group_avc_ime_get_inter_distortions( result );
    uchar major_shape  = intel_sub_group_avc_ime_get_inter_major_shape( result );
    uchar minor_shapes = intel_sub_group_avc_ime_get_inter_minor_shapes( result );
    uchar directions   = intel_sub_group_avc_ime_get_inter_directions( result );

    // Perform FME for sub-pixel resolution results.

    if( pixel_mode != CLK_AVC_ME_SUBPIXEL_MODE_INTEGER_INTEL ) {
        intel_sub_group_avc_ref_payload_t payload = intel_sub_group_avc_fme_initialize( srcCoord, mvs, major_shape, minor_shapes, directions, pixel_mode, sad_adjustment );
        payload = intel_sub_group_avc_ref_set_motion_vector_cost_function( cost_center, packed_cost_table, search_cost_precision, payload );

        intel_sub_group_avc_ref_result_t result = intel_sub_group_avc_ref_evaluate_with_single_reference( srcImg, refImg, accelerator, payload );
        dists = intel_sub_group_avc_ref_get_inter_distortions( result );
        mvs = intel_sub_group_avc_ref_get_motion_vectors( result );
    }   

    uchar2 shapes = { major_shape, minor_shapes };
    
    // Write out results.
    int index = ( mbid.x * 16 + get_local_id(0) ) + ( mbid.y * 16 * ngrp.x );
    int2 bi_mvs = as_int2( mvs );
    motion_vector_buffer [ index ] = as_short2( bi_mvs.s0 );

    if( residuals_buffer != NULL ) {
        residuals_buffer [ index ] = dists;
    }

    shapes_buffer [mbid.x + mbid.y * ngrp.x] = shapes;

#if USE_OPENCL_2_0_ATOMICS	
	// The acquire-release semantics of the atomic operations used for the poll 
	// and signal operation will ensure that the writes are visible to other
	// threads prior to the scoreboard  updates becoming visible.
#else
	// Ensure that the writes are visible to other threads prior to the
    // scoreboard updates becoming visible.
    write_mem_fence(CLK_GLOBAL_MEM_FENCE);	     
#endif

    // ....................Signaling phase.................................
    if( get_sub_group_local_id() == 0 )
    {
        signal_scoreboard(mbid, ngrp,scoreboard);
    }
}
