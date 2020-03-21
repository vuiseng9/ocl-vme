/*****************************************************************************\
  Dependency bit settings
\*****************************************************************************/

#define TOP_DEP         (1 << 0)
#define TOP_LEFT_DEP    (1 << 1)
#define LEFT_DEP        (1 << 2)
#define ALL_DEP         (TOP_DEP | TOP_LEFT_DEP | LEFT_DEP)

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
void poll_scoreboard(int2 mbid, int2 ngrp, __global  atomic_int* scoreboard)
{
    int s = 0;
    while (s != ALL_DEP)
    {	
      s = 
		atomic_load_explicit( 
			scoreboard + mbid.y * ngrp.x + mbid.x, 
			memory_order_acquire,
			memory_scope_device);
    }
}

/*****************************************************************************\
Function:
    signal_scoreboard

Description:
    Clear mbid's dependency in the scoreboard entries of its depending MB.
\*****************************************************************************/
void signal_scoreboard(int2 mbid, int2 ngrp,__global  atomic_int* scoreboard)
{
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
}

/*****************************************************************************\
Kernel Function:
    downsample_4x

Description:
    Performs downsampling of the input image by 4x using intel subgroup media
    block read and write.
\*****************************************************************************/
__kernel  __attribute__((reqd_work_group_size(16,1,1))) __attribute__((intel_reqd_sub_group_size(16)))
void downsample4x(
    __read_only image2d_t input,
    __write_only image2d_t output)
{

    int x = get_group_id(0) * 16 * 4;
    int y = get_group_id(1)      * 16;

    uint8 p0, p1;
    uint sum2;
    p0 = intel_sub_group_block_read8(input, (int2)(x, y));
    p1 = intel_sub_group_block_read8(input, (int2)(x, y+8));
        
    uchar4 r0 = as_uchar4(p0.s0);
    uchar4 r1 = as_uchar4(p0.s1);
    uchar4 r2 = as_uchar4(p0.s2);
    uchar4 r3 = as_uchar4(p0.s3);

    uchar4 r4 = as_uchar4(p0.s4);
    uchar4 r5 = as_uchar4(p0.s5);
    uchar4 r6 = as_uchar4(p0.s6);
    uchar4 r7 = as_uchar4(p0.s7);

    uchar4 r8 = as_uchar4(p1.s0);
    uchar4 r9 = as_uchar4(p1.s1);
    uchar4 r10 = as_uchar4(p1.s2);
    uchar4 r11 = as_uchar4(p1.s3);

    uchar4 r12 = as_uchar4(p1.s4);
    uchar4 r13 = as_uchar4(p1.s5);
    uchar4 r14 = as_uchar4(p1.s6);
    uchar4 r15 = as_uchar4(p1.s7);
        
    uchar16 t1r;
    t1r.s0 = (r0.s0 + r0.s1 + r1.s0 + r1.s1 + 2) / 4;
    t1r.s1 = (r0.s2 + r0.s3 + r1.s2 + r1.s3 + 2) / 4;

    t1r.s2 = (r2.s0 + r2.s1 + r3.s0 + r3.s1 + 2) / 4;
    t1r.s3 = (r2.s2 + r2.s3 + r3.s2 + r3.s3 + 2) / 4;

    t1r.s4 = (r4.s0 + r4.s1 + r5.s0 + r5.s1 + 2) / 4;
    t1r.s5 = (r4.s2 + r4.s3 + r5.s2 + r5.s3 + 2) / 4;

    t1r.s6 = (r6.s0 + r6.s1 + r7.s0 + r7.s1 + 2) / 4;
    t1r.s7 = (r6.s2 + r6.s3 + r7.s2 + r7.s3 + 2) / 4;

    t1r.s8 = (r8.s0 + r8.s1 + r9.s0 + r9.s1 + 2) / 4;
    t1r.s9 = (r8.s2 + r8.s3 + r9.s2 + r9.s3 + 2) / 4;

    t1r.sa = (r10.s0 + r10.s1 + r11.s0 + r11.s1 + 2) / 4;
    t1r.sb = (r10.s2 + r10.s3 + r11.s2 + r11.s3 + 2) / 4;

    t1r.sc = (r12.s0 + r12.s1 + r13.s0 + r13.s1 + 2) / 4;
    t1r.sd = (r12.s2 + r12.s3 + r13.s2 + r13.s3 + 2) / 4;

    t1r.se = (r14.s0 + r14.s1 + r15.s0 + r15.s1 + 2) / 4;
    t1r.sf = (r14.s2 + r14.s3 + r15.s2 + r15.s3 + 2) / 4;

    int xg = x / 4;
    int yg = y / 4;

    uchar4 t2;
    t2.s0 = (t1r.s0 + t1r.s1 + t1r.s2 + t1r.s3 + 2) / 4;
    t2.s1 = (t1r.s4 + t1r.s5 + t1r.s6 + t1r.s7 + 2) / 4;
    t2.s2 = (t1r.s8 + t1r.s9 + t1r.sa + t1r.sb + 2) / 4;
    t2.s3 = (t1r.sc + t1r.sd + t1r.se + t1r.sf + 2) / 4;

    intel_sub_group_media_block_write_uc4((int2)(xg, yg), 16, 4, t2, output);
}

/*****************************************************************************\
Helper Function:
    tier1_ime_4x

Description:
    Performs tier1 basic IME operation on the 4x downscaled frames using only
    4x4 block partitions.  
\*****************************************************************************/
inline intel_sub_group_avc_ime_result_t tier1_ime_4x(
    __read_only image2d_t src_img,
    __read_only image2d_t ref_img,
    ushort2 src_coord,
    short2 ref_coord )
{
    sampler_t accelerator     = CLK_AVC_ME_INITIALIZE_INTEL;
    uchar sad_adjustment      = CLK_AVC_ME_SAD_ADJUST_MODE_NONE_INTEL;    
    uchar partition_mask      = CLK_AVC_ME_PARTITION_MASK_4x4_INTEL;

    // Ensure that the reference search window is atleast partially within the reference frame.
    ushort2 framesize = convert_ushort2(get_image_dim(src_img));
    ushort2 searchSize = intel_sub_group_ime_ref_window_size(CLK_AVC_ME_SEARCH_WINDOW_EXHAUSTIVE_INTEL, 0);
    ref_coord = intel_sub_group_avc_ime_adjust_ref_offset(ref_coord, src_coord, searchSize, framesize); 

    intel_sub_group_avc_ime_payload_t payload;
    payload = intel_sub_group_avc_ime_initialize( src_coord, partition_mask, sad_adjustment );
    payload = 
        intel_sub_group_avc_ime_set_single_reference( 
            ref_coord, CLK_AVC_ME_SEARCH_WINDOW_EXHAUSTIVE_INTEL, payload );
    intel_sub_group_avc_ime_result_t result; 
    result = intel_sub_group_avc_ime_evaluate_with_single_reference( src_img, ref_img, accelerator, payload );

    return result;
}

/*****************************************************************************\
Kernel Function:
    tier1_block_motion_estimate_intel

Description:
    Performs simple block motion estimation with single reference frame on a 
    4x downsampled image using 1/4 pixel resolution.

\*****************************************************************************/
__kernel __attribute__((reqd_work_group_size(16,1,1)))
void tier1_block_motion_estimate_intel(
    __read_only image2d_t   src_img,
    __read_only image2d_t   ref_img,
    __global short2*        motion_vector_buffer ) 
{
    int2 gid = { get_group_id(0), get_group_id(1) };
    int2 ngrp = { get_num_groups(0), get_num_groups(1) };
    ushort2 searchSize = intel_sub_group_ime_ref_window_size(CLK_AVC_ME_SEARCH_WINDOW_EXHAUSTIVE_INTEL, 0);

    ushort2 src_coord = 0;
    short2 ref_coord = 0;

    src_coord.x = gid.x * 16;
    src_coord.y = gid.y * 16;

    // Effectively search a 48*3 x 40*3 search area on the 4x downsampled image.
    
    intel_sub_group_avc_ime_result_t result; 
    
    result = tier1_ime_4x(src_img, ref_img, src_coord, ref_coord );
    
    long mvs = intel_sub_group_avc_ime_get_motion_vectors( result );
    ushort dists = intel_sub_group_avc_ime_get_inter_distortions( result );
    
    long mvsn = mvs;
    ushort distsn = dists;
    
    ref_coord.x = 0; ref_coord.y = -searchSize.y;
    result = tier1_ime_4x(src_img, ref_img, src_coord, ref_coord );
    mvsn = intel_sub_group_avc_ime_get_motion_vectors( result );
    distsn = intel_sub_group_avc_ime_get_inter_distortions( result );
    if( distsn < dists ) {
        mvs = mvsn;
        dists = distsn;
    }

    ref_coord.x = 0; ref_coord.y = +searchSize.y;
    result = tier1_ime_4x(src_img, ref_img, src_coord, ref_coord );
    mvsn = intel_sub_group_avc_ime_get_motion_vectors( result );
    distsn = intel_sub_group_avc_ime_get_inter_distortions( result );
    if( distsn < dists ) {
        mvs = mvsn;
        dists = distsn;
    }

    ref_coord.x = -searchSize.x; ref_coord.y = 0;
    result = tier1_ime_4x(src_img, ref_img, src_coord, ref_coord );
    mvsn = intel_sub_group_avc_ime_get_motion_vectors( result );
    distsn = intel_sub_group_avc_ime_get_inter_distortions( result );
    if( distsn < dists ) {
        mvs = mvsn;
        dists = distsn;
    }

    ref_coord.x = +searchSize.x; ref_coord.y = 0;
    result = tier1_ime_4x(src_img, ref_img, src_coord, ref_coord );
    mvsn = intel_sub_group_avc_ime_get_motion_vectors( result );
    distsn = intel_sub_group_avc_ime_get_inter_distortions( result );
    if( distsn < dists ) {
        mvs = mvsn;
        dists = distsn;
    }

    ref_coord.x = -searchSize.x; ref_coord.y = -searchSize.y;
    result = tier1_ime_4x(src_img, ref_img, src_coord, ref_coord );
    mvsn = intel_sub_group_avc_ime_get_motion_vectors( result );
    distsn = intel_sub_group_avc_ime_get_inter_distortions( result );
    if( distsn < dists ) {
        mvs = mvsn;
        dists = distsn;
    }

    ref_coord.x = -searchSize.x; ref_coord.y = +searchSize.y;
    result = tier1_ime_4x(src_img, ref_img, src_coord, ref_coord );
    mvsn = intel_sub_group_avc_ime_get_motion_vectors( result );
    distsn = intel_sub_group_avc_ime_get_inter_distortions( result );
    if( distsn < dists ) {
        mvs = mvsn;
        dists = distsn;
    }

    ref_coord.x = +searchSize.x; ref_coord.y = -searchSize.y;
    result = tier1_ime_4x(src_img, ref_img, src_coord, ref_coord );
    mvsn = intel_sub_group_avc_ime_get_motion_vectors( result );
    distsn = intel_sub_group_avc_ime_get_inter_distortions( result );
    if( distsn < dists ) {
        mvs = mvsn;
        dists = distsn;
    }

    ref_coord.x = +searchSize.x; ref_coord.y = +searchSize.y;
    result = tier1_ime_4x(src_img, ref_img, src_coord, ref_coord );
    mvsn = intel_sub_group_avc_ime_get_motion_vectors( result );
    distsn = intel_sub_group_avc_ime_get_inter_distortions( result );
    if( distsn < dists ) {
        mvs = mvsn;
        dists = distsn;
    }

    // Perform 1/4 pixel resolution on 4x downscaled imagesn, full pixel on original images.

    sampler_t accelerator = CLK_AVC_ME_INITIALIZE_INTEL;
    uchar sub_pixel_mode = CLK_AVC_ME_SUBPIXEL_MODE_QPEL_INTEL;
    uchar sad_adjustment = CLK_AVC_ME_SAD_ADJUST_MODE_NONE_INTEL;   
    uchar major_shape = CLK_AVC_ME_MAJOR_8x8_INTEL;
    uchar minor_shapes = CLK_AVC_ME_MINOR_4x4_INTEL;
    uchar directions = 0;

    intel_sub_group_avc_ref_payload_t payload;
    payload = 
        intel_sub_group_avc_fme_initialize( 
            src_coord, mvs, major_shape, minor_shapes, directions, sub_pixel_mode, sad_adjustment );
            
    intel_sub_group_avc_ref_result_t result_ref;
    result_ref = intel_sub_group_avc_ref_evaluate_with_single_reference( src_img, ref_img, accelerator, payload );
    mvs = intel_sub_group_avc_ref_get_motion_vectors( result_ref );

    // Order the 4x4 block results to use as predictors with 4x scaling for the original 16x16 blocks.

    int sub_rows = get_sub_group_local_id() / 4;
    int sub_cols = get_sub_group_local_id() % 4;
    int index = ( gid.x * 4 + sub_cols) + ( gid.y * 4 + sub_rows ) * ngrp.x * 4;                                           
    
    int2 bi_mvs = as_int2( mvs );
    motion_vector_buffer [ index ] = as_short2( bi_mvs.s0 ) << 2;
}

/*****************************************************************************\
Function:
    get_cost_center

Description:
    Get the cost center corresponding to the input integer offset.
\*****************************************************************************/
ulong get_cost_center(short2 value) 
{
    short2 costCoord;
    // Cost coords are in QPEL resolution.
    costCoord.s0 = value.s0 << 2;
    costCoord.s1 = value.s1 << 2;
    uint2 cost_center_int;
    cost_center_int.s0 = as_uint( costCoord );
    cost_center_int.s1 = 0;
    return as_ulong( cost_center_int );
}

inline intel_sub_group_avc_ime_result_single_reference_streamout_t 
ime_streamout(
    __read_only image2d_t src_img,
    __read_only image2d_t ref_img,
    ushort2 src_coord,
    short2 ref_coord,
    uint partition_mask,
    uchar sad_adjustment,
    ulong cost_center,
    uchar search_cost_precision,
    uint2 packed_cost_table,
    ulong shape_penalty,
    uchar ref_penalty,
    sampler_t accelerator )

{   
    // Ensure that the reference search window is atleast partially within the reference frame.
    ushort2 framesize = convert_ushort2(get_image_dim(src_img));
    ushort2 searchSize = intel_sub_group_ime_ref_window_size(CLK_AVC_ME_SEARCH_WINDOW_EXHAUSTIVE_INTEL, 0);
    ref_coord = intel_sub_group_avc_ime_adjust_ref_offset(ref_coord, src_coord, searchSize, framesize); 
   
    intel_sub_group_avc_ime_payload_t payload;
    intel_sub_group_avc_ime_result_single_reference_streamout_t resultsout;
    intel_sub_group_avc_ime_single_reference_streamin_t resultsin;
    
    payload = intel_sub_group_avc_ime_initialize( src_coord, partition_mask, sad_adjustment );
    payload = intel_sub_group_avc_ime_set_single_reference( ref_coord, CLK_AVC_ME_SEARCH_WINDOW_EXHAUSTIVE_INTEL, payload );
    payload = intel_sub_group_avc_ime_set_motion_vector_cost_function( cost_center, packed_cost_table, search_cost_precision, payload );
    payload = intel_sub_group_avc_ime_set_inter_base_multi_reference_penalty( ref_penalty, payload );
    payload = intel_sub_group_avc_ime_set_inter_shape_penalty( shape_penalty, payload );
    resultsout = intel_sub_group_avc_ime_evaluate_with_single_reference_streamout( src_img, ref_img, accelerator, payload );      
    
    return resultsout;
}

inline intel_sub_group_avc_ime_result_single_reference_streamout_t 
ime_streaminout(
    __read_only image2d_t src_img,
    __read_only image2d_t ref_img,
    ushort2 src_coord,
    short2 ref_coord,
    uint partition_mask,
    uchar sad_adjustment,
    ulong cost_center,
    uchar search_cost_precision,
    uint2 packed_cost_table,
    ulong shape_penalty,
    uchar ref_penalty,
    intel_sub_group_avc_ime_result_single_reference_streamout_t resultsout,
    sampler_t accelerator )

{    
    // Ensure that the reference search window is atleast partially within the reference frame.
    ushort2 framesize = convert_ushort2(get_image_dim(src_img));
    ushort2 searchSize = intel_sub_group_ime_ref_window_size(CLK_AVC_ME_SEARCH_WINDOW_EXHAUSTIVE_INTEL, 0);
    ref_coord = intel_sub_group_avc_ime_adjust_ref_offset(ref_coord, src_coord, searchSize, framesize); 
   
    intel_sub_group_avc_ime_payload_t payload;
    intel_sub_group_avc_ime_single_reference_streamin_t resultsin;
          
    resultsin = intel_sub_group_avc_ime_get_single_reference_streamin( resultsout );
    payload = intel_sub_group_avc_ime_initialize( src_coord, partition_mask, sad_adjustment );
    payload = intel_sub_group_avc_ime_set_single_reference( ref_coord, CLK_AVC_ME_SEARCH_WINDOW_EXHAUSTIVE_INTEL, payload );
    payload = intel_sub_group_avc_ime_set_motion_vector_cost_function( cost_center, packed_cost_table, search_cost_precision, payload );
    payload = intel_sub_group_avc_ime_set_inter_base_multi_reference_penalty( ref_penalty, payload );
    payload = intel_sub_group_avc_ime_set_inter_shape_penalty( shape_penalty, payload );
    resultsout = intel_sub_group_avc_ime_evaluate_with_single_reference_streaminout( src_img, ref_img, accelerator, payload, resultsin );
    return resultsout;
}

inline intel_sub_group_avc_ime_result_single_reference_streamout_t 
ime_streaminoutframe_no_zero_pred(
    __read_only image2d_t src_img,
    __read_only image2d_t ref_img,
    ushort2 src_coord,
    short2 ref0_coord,
    short2 ref1_coord,
    short2 ref2_coord,
    short2 ref3_coord,
    uint pred_count,
    uint partition_mask,
    uchar sad_adjustment,
    ulong cost_center0,
    ulong cost_center1,
    ulong cost_center2,
    ulong cost_center3,
    uchar search_cost_precision,
    uint2 packed_cost_table,
    ulong shape_penalty,
    uchar ref_penalty,
    intel_sub_group_avc_ime_result_single_reference_streamout_t resultsout,
    sampler_t accelerator )
{
    if( pred_count == 1 ) {
        resultsout = ime_streaminout(
            src_img, ref_img, src_coord, ref0_coord, partition_mask, sad_adjustment, 
            cost_center0, search_cost_precision, packed_cost_table,
            shape_penalty, ref_penalty,
            resultsout,
            accelerator );
    }
    else if( pred_count == 2 ) {
        resultsout = ime_streaminout(
            src_img, ref_img, src_coord, ref0_coord, partition_mask, sad_adjustment, 
            cost_center0, search_cost_precision, packed_cost_table,
            shape_penalty, ref_penalty,
            resultsout,
            accelerator );
        resultsout = ime_streaminout(
            src_img, ref_img, src_coord, ref1_coord, partition_mask, sad_adjustment, 
            cost_center1, search_cost_precision, packed_cost_table,
            shape_penalty, ref_penalty,
            resultsout,
            accelerator );
    }
    else if( pred_count == 3 ) {
        resultsout = ime_streaminout(
            src_img, ref_img, src_coord, ref0_coord, partition_mask, sad_adjustment, 
            cost_center0, search_cost_precision, packed_cost_table,
            shape_penalty, ref_penalty,
            resultsout,
            accelerator );
        resultsout = ime_streaminout(
            src_img, ref_img, src_coord, ref1_coord, partition_mask, sad_adjustment, 
            cost_center1, search_cost_precision, packed_cost_table,
            shape_penalty, ref_penalty,
            resultsout,
            accelerator );
        resultsout = ime_streaminout(
            src_img, ref_img, src_coord, ref2_coord, partition_mask, sad_adjustment, 
            cost_center2, search_cost_precision, packed_cost_table,
            shape_penalty, ref_penalty,
            resultsout,
            accelerator );
    }
    else {
        resultsout = ime_streaminout(
            src_img, ref_img, src_coord, ref0_coord, partition_mask, sad_adjustment, 
            cost_center0, search_cost_precision, packed_cost_table,
            shape_penalty, ref_penalty,
            resultsout,
            accelerator );
        resultsout = ime_streaminout(
            src_img, ref_img, src_coord, ref1_coord, partition_mask, sad_adjustment, 
            cost_center1, search_cost_precision, packed_cost_table,
            shape_penalty, ref_penalty,
            resultsout,
            accelerator );
        resultsout = ime_streaminout(
            src_img, ref_img, src_coord, ref2_coord, partition_mask, sad_adjustment, 
            cost_center2, search_cost_precision, packed_cost_table,
            shape_penalty, ref_penalty,
            resultsout,
            accelerator );
        resultsout = ime_streaminout(
            src_img, ref_img, src_coord, ref3_coord, partition_mask, sad_adjustment, 
            cost_center3, search_cost_precision, packed_cost_table,
            shape_penalty, ref_penalty,
            resultsout,
            accelerator );
    }

    return resultsout;
}

inline intel_sub_group_avc_ime_result_single_reference_streamout_t 
ime_streamoutframe(
    __read_only image2d_t src_img,
    __read_only image2d_t ref_img,
    ushort2 src_coord,
    short2 ref0_coord,
    short2 ref1_coord,
    short2 ref2_coord,
    short2 ref3_coord,
    uint pred_count,
    uint partition_mask,
    uchar sad_adjustment,
    ulong cost_center0,
    ulong cost_center1,
    ulong cost_center2,
    ulong cost_center3,
    uchar search_cost_precision,
    uint2 packed_cost_table,
    ulong shape_penalty,
    uchar ref_penalty,
    sampler_t accelerator )
{
    intel_sub_group_avc_ime_result_single_reference_streamout_t resultsout;
    short2 zero_ref_coord = 0;
    ulong zero_cost_center = 0;
    resultsout = ime_streamout(
        src_img, ref_img, src_coord, zero_ref_coord, partition_mask, sad_adjustment, 
        zero_cost_center, search_cost_precision, packed_cost_table,
        shape_penalty, ref_penalty,
        accelerator );
    resultsout = ime_streaminoutframe_no_zero_pred(
        src_img, ref_img, src_coord, 
        ref0_coord, ref1_coord, ref2_coord, ref3_coord, pred_count,
        partition_mask, sad_adjustment, 
        cost_center0, cost_center1, cost_center2, cost_center2, 
        search_cost_precision, packed_cost_table,
        shape_penalty, ref_penalty,
        resultsout,
        accelerator );
    
    return resultsout;
}

inline intel_sub_group_avc_ime_result_single_reference_streamout_t 
ime_streaminoutframe(
    __read_only image2d_t src_img,
    __read_only image2d_t ref_img,
    ushort2 src_coord,
    short2 ref0_coord,
    short2 ref1_coord,
    short2 ref2_coord,
    short2 ref3_coord,
    uint pred_count,
    uint partition_mask,
    uchar sad_adjustment,
    ulong cost_center0,
    ulong cost_center1,
    ulong cost_center2,
    ulong cost_center3,
    uchar search_cost_precision,
    uint2 packed_cost_table,
    ulong shape_penalty,
    uchar ref_penalty,
    intel_sub_group_avc_ime_result_single_reference_streamout_t resultsout,
    sampler_t accelerator )
{
    short2 zero_ref_coord = 0;
    ulong zero_cost_center = 0;
    resultsout = ime_streaminout(
        src_img, ref_img, src_coord, zero_ref_coord, partition_mask, sad_adjustment, 
        zero_cost_center, search_cost_precision, packed_cost_table,
        shape_penalty, ref_penalty,
        resultsout,
        accelerator );
    resultsout = ime_streaminoutframe_no_zero_pred(
        src_img, ref_img, src_coord, 
        ref0_coord, ref1_coord, ref2_coord, ref3_coord, pred_count,
        partition_mask, sad_adjustment, 
        cost_center0, cost_center1, cost_center2, cost_center2, 
        search_cost_precision, packed_cost_table,
        shape_penalty, ref_penalty,
        resultsout,
        accelerator );

    return resultsout;
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
    __read_only image2d_t   src_img,        // src

    __read_only image2d_t   ref00_img,      // ref0 fwd
    __read_only image2d_t   ref01_img,   

    __read_only image2d_t   ref10_img,      // ref1 fwd
    __read_only image2d_t   ref11Img,

    __read_only image2d_t   ref20_img,      // ref2 fwd
    __read_only image2d_t   ref21_img,

    __read_only image2d_t   ref30_img,      // ref3 fwd
    __read_only image2d_t   ref31_img,   

    __read_only image2d_t   ref40_img,      // ref4 fwd
    __read_only image2d_t   ref41Img,

    __read_only image2d_t   ref50_img,      // ref5 fwd
    __read_only image2d_t   ref51_img,

    __read_only image2d_t   ref60_img,      // ref6 fwd
    __read_only image2d_t   ref61_img,

    __read_only image2d_t   ref70_img,      // ref7 fwd
    __read_only image2d_t   ref71_img,   

    __read_only image2d_t   ref80_img,      // ref8 fwd
    __read_only image2d_t   ref81Img,

    __read_only image2d_t   ref90_img,      // ref9 fwd
    __read_only image2d_t   ref91_img,

    __read_only image2d_t   refA0_img,      // refA fwd
    __read_only image2d_t   refA1_img,   

    __read_only image2d_t   refB0_img,      // refA fwd
    __read_only image2d_t   refB1Img,

    __read_only image2d_t   refC0_img,      // refB fwd
    __read_only image2d_t   refC1_img,

    __read_only image2d_t   refD0_img,      // refC fwd
    __read_only image2d_t   refD1_img,   

    __read_only image2d_t   refE0_img,      // refD fwd
    __read_only image2d_t   refE1Img,

    __read_only image2d_t   refF0_img,      // refF fwd
    __read_only image2d_t   refF1_img,

    __read_only image2d_t   src_luma_img,   // intra luma src

    uint                    num_use_refs,   // num refs to use

     __global short2*       predictor_buffer,

    __global short2*        motion_vector_buffer,
    __global ushort*        residuals_buffer,
    __global ushort*        best_residual_buffer,
    __global uchar2*        shapes_buffer,
    __global uint*          reference_id_buffer,

	__global uchar *        intra_luma_shape,
	__global ushort*        intra_luma_residuals,
    __global ulong*         intra_luma_modes,

    __global atomic_int*    scoreboard,
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
		poll_scoreboard( mbid, ngrp, scoreboard );
	}

    // ....................Processing phase.................................

    sampler_t accelerator       = CLK_AVC_ME_INITIALIZE_INTEL;
    uchar inter_partition_mask  = CLK_AVC_ME_PARTITION_MASK_ALL_INTEL;
    uchar sad_adjustment        = CLK_AVC_ME_SAD_ADJUST_MODE_NONE_INTEL;
    uchar sub_pixel_mode        = CLK_AVC_ME_SUBPIXEL_MODE_QPEL_INTEL;
    uchar search_cost_precision = CLK_AVC_ME_COST_PRECISION_QPEL_INTEL; 
    uchar slice_type            = CLK_AVC_ME_SLICE_TYPE_PRED_INTEL;
    uchar qp                    = 45;
    
    ushort2 srcCoord;
    srcCoord.x = mbid.x * 16;
    srcCoord.y = mbid.y * 16;
        
    long mvs           = 0;
    ushort dists       = 0;
    ushort best_dist   = 0;
    uchar major_shape  = 0;
    uchar minor_shapes = 0;
    uchar2 shapes      = 0;
    uchar directions   = 0;
    uint ref_ids       = 0;

    //------------------- Perfrom inter estimation -------------------------

    if (num_use_refs > 0) {
        uint predCount = 0;
        short2 predMV0 = 0, predMV1 = 0, predMV2 = 0, predMV3 = 0;
    
        if (mbid.x == 0 && mbid.y == 0) {
            predMV0 = predictor_buffer [ mbid.x + mbid.y * ngrp.x ];
            predCount = 1;
        }
        else if (mbid.x == 0) {        
            predMV0 = motion_vector_buffer [ mbid.x * 16 + (mbid.y - 1) * 16 * ngrp.x ];
            predMV1 = predictor_buffer [ mbid.x + mbid.y * ngrp.x ];
            predCount = 2;
        }
        else if (mbid.y == 0) {
            predMV0 = motion_vector_buffer [ (mbid.x - 1) * 16 + mbid.y * 16 * ngrp.x ];
            predMV1 = predictor_buffer [ mbid.x + mbid.y * ngrp.x ];
            predCount = 2;
        }
        else {
            predMV0 = motion_vector_buffer [ (mbid.x - 1) * 16 + (mbid.y - 1) * 16 * ngrp.x ];
            predMV1 = motion_vector_buffer [ (mbid.x * 16) + (mbid.y - 1) * 16 * ngrp.x ];
            predMV2 = motion_vector_buffer [ (mbid.x - 1) * 16 + mbid.y * 16 * ngrp.x ];
            predMV3 = predictor_buffer [ mbid.x + mbid.y * ngrp.x ];
            predCount = 4;
        }

        short2 refCoord0, refCoord1, refCoord2, refCoord3;        

        srcCoord.x = mbid.x * 16;
        srcCoord.y = mbid.y * 16;

        // Obtain refCoord in PEL resolution and cost center in QPEL resolution.

        refCoord0 = predMV0 >> 2; 
        refCoord1 = predMV1 >> 2;
        refCoord2 = predMV2 >> 2;
        refCoord3 = predMV3 >> 2;
    
        // Obtain cost centers.

        ulong cost_center0, cost_center1, cost_center2, cost_center3;       
        
        cost_center0 = get_cost_center(refCoord0);
        cost_center0 = get_cost_center(refCoord0);
        cost_center0 = get_cost_center(refCoord0);
        cost_center0 = get_cost_center(refCoord0);

        uint2 packed_cost_table = intel_sub_group_avc_mce_get_default_high_penalty_cost_table();    

        ulong shape_penalty = intel_sub_group_avc_mce_get_default_inter_shape_penalty( slice_type, qp );
        uchar ref_penalty = intel_sub_group_avc_mce_get_default_inter_base_multi_reference_penalty( slice_type, qp );

        intel_sub_group_avc_ime_result_single_reference_streamout_t resultsout;
        intel_sub_group_avc_ime_result_t result;
   
        // Perform IME for 4 different search areas based on neighbor predictors for ref0.

        resultsout = ime_streamoutframe(
            src_img, ref00_img, srcCoord, 
            refCoord0, refCoord1, refCoord2, refCoord3, predCount,
            inter_partition_mask, sad_adjustment, 
            cost_center0, cost_center1, cost_center2, cost_center3, 
            search_cost_precision, packed_cost_table,
            shape_penalty, ref_penalty,
            accelerator );

        // Perform IME for 4 different search areas based on neighbor predictors for ref1.

        if( num_use_refs > 1 ) {
            resultsout = ime_streaminoutframe(
                src_img, ref10_img, srcCoord, 
                refCoord0, refCoord1, refCoord2, refCoord3, predCount,
                inter_partition_mask, sad_adjustment, 
                cost_center0, cost_center1, cost_center2, cost_center3,  
                search_cost_precision, packed_cost_table,
                shape_penalty, ref_penalty,
                resultsout,
                accelerator );
        }

        // Perform IME for 4 different search areas based on neighbor predictors for ref2.

        if( num_use_refs > 2 ) {
            resultsout = ime_streaminoutframe(
                src_img, ref20_img, srcCoord, 
                refCoord0, refCoord1, refCoord2, refCoord3, predCount,
                inter_partition_mask, sad_adjustment, 
                cost_center0, cost_center1, cost_center2, cost_center3, 
                search_cost_precision, packed_cost_table,
                shape_penalty, ref_penalty,
                resultsout,
                accelerator );
        }

        // Perform IME for 4 different search areas based on neighbor predictors for ref3.

        if( num_use_refs > 3 ) {
            resultsout = ime_streaminoutframe(
                src_img, ref30_img, srcCoord, 
                refCoord0, refCoord1, refCoord2, refCoord3, predCount,
                inter_partition_mask, sad_adjustment, 
                cost_center0, cost_center1, cost_center2, cost_center3, 
                search_cost_precision, packed_cost_table,
                shape_penalty, ref_penalty,
                resultsout,
                accelerator );
        }

        // Perform IME for 4 different search areas based on neighbor predictors for ref4.

        if( num_use_refs > 4 ) {
            resultsout = ime_streaminoutframe(
                src_img, ref40_img, srcCoord, 
                refCoord0, refCoord1, refCoord2, refCoord3, predCount,
                inter_partition_mask, sad_adjustment, 
                cost_center0, cost_center1, cost_center2, cost_center3, 
                search_cost_precision, packed_cost_table,
                shape_penalty, ref_penalty,
                resultsout,
                accelerator );
        }

        // Perform IME for 4 different search areas based on neighbor predictors for ref5.

        if( num_use_refs > 5 ) {
            resultsout = ime_streaminoutframe(
                src_img, ref50_img, srcCoord, 
                refCoord0, refCoord1, refCoord2, refCoord3, predCount,
                inter_partition_mask, sad_adjustment, 
                cost_center0, cost_center1, cost_center2, cost_center3, 
                search_cost_precision, packed_cost_table,
                shape_penalty, ref_penalty,
                resultsout,
                accelerator );
            }

        // Perform IME for 4 different search areas based on neighbor predictors for ref6.

        if( num_use_refs > 6 ) {
            resultsout = ime_streaminoutframe(
                src_img, ref60_img, srcCoord, 
                refCoord0, refCoord1, refCoord2, refCoord3, predCount,
                inter_partition_mask, sad_adjustment, 
                cost_center0, cost_center1, cost_center2, cost_center3, 
                search_cost_precision, packed_cost_table,
                shape_penalty, ref_penalty,
                resultsout,
                accelerator );
        }

        // Perform IME for 4 different search areas based on neighbor predictors for ref7.

        if( num_use_refs > 7 ) {
            resultsout = ime_streaminoutframe(
                src_img, ref70_img, srcCoord, 
                refCoord0, refCoord1, refCoord2, refCoord3, predCount,
                inter_partition_mask, sad_adjustment, 
                cost_center0, cost_center1, cost_center2, cost_center3,  
                search_cost_precision, packed_cost_table,
                shape_penalty, ref_penalty,
                resultsout,
                accelerator );
        }

        // Perform IME for 4 different search areas based on neighbor predictors for ref8.

        if( num_use_refs > 8 ) {
            resultsout = ime_streaminoutframe(
                src_img, ref80_img, srcCoord, 
                refCoord0, refCoord1, refCoord2, refCoord3, predCount,
                inter_partition_mask, sad_adjustment, 
                cost_center0, cost_center1, cost_center2, cost_center3, 
                search_cost_precision, packed_cost_table,
                shape_penalty, ref_penalty,
                resultsout,
                accelerator );
        }

        // Perform IME for 4 different search areas based on neighbor predictors for ref9.

        if( num_use_refs > 9 ) {
            resultsout = ime_streaminoutframe(
                src_img, ref90_img, srcCoord, 
                refCoord0, refCoord1, refCoord2, refCoord3, predCount,
                inter_partition_mask, sad_adjustment, 
                cost_center0, cost_center1, cost_center2, cost_center3, 
                search_cost_precision, packed_cost_table,
                shape_penalty, ref_penalty,
                resultsout,
                accelerator );
        }

        // Perform IME for 4 different search areas based on neighbor predictors for ref10.

        if( num_use_refs > 0xA ) {
            resultsout = ime_streaminoutframe(
                src_img, refA0_img, srcCoord, 
                refCoord0, refCoord1, refCoord2, refCoord3, predCount,
                inter_partition_mask, sad_adjustment, 
                cost_center0, cost_center1, cost_center2, cost_center3, 
                search_cost_precision, packed_cost_table,
                shape_penalty, ref_penalty,
                resultsout,
                accelerator );
        }

        // Perform IME for 4 different search areas based on neighbor predictors for ref11.

        if( num_use_refs > 0xB ) {
            resultsout = ime_streaminoutframe(
                src_img, refB0_img, srcCoord, 
                refCoord0, refCoord1, refCoord2, refCoord3, predCount,
                inter_partition_mask, sad_adjustment, 
                cost_center0, cost_center1, cost_center2, cost_center3, 
                search_cost_precision, packed_cost_table,
                shape_penalty, ref_penalty,
                resultsout,
                accelerator );
        }

        // Perform IME for 4 different search areas based on neighbor predictors for ref12.

        if( num_use_refs > 0xC ) {
            resultsout = ime_streaminoutframe(
                src_img, refC0_img, srcCoord, 
                refCoord0, refCoord1, refCoord2, refCoord3, predCount,
                inter_partition_mask, sad_adjustment, 
                cost_center0, cost_center1, cost_center2, cost_center3, 
                search_cost_precision, packed_cost_table,
                shape_penalty, ref_penalty,
                resultsout,
                accelerator );
        }

        // Perform IME for 4 different search areas based on neighbor predictors for ref13.

        if( num_use_refs > 0xD ) {
            resultsout = ime_streaminoutframe(
                src_img, refD0_img, srcCoord, 
                refCoord0, refCoord1, refCoord2, refCoord3, predCount,
                inter_partition_mask, sad_adjustment, 
                cost_center0, cost_center1, cost_center2, cost_center3, 
                search_cost_precision, packed_cost_table,
                shape_penalty, ref_penalty,
                resultsout,
                accelerator );
        }

        // Perform IME for 4 different search areas based on neighbor predictors for ref14.

        if( num_use_refs > 0xE ) {
            resultsout = ime_streaminoutframe(
                src_img, refE0_img, srcCoord, 
                refCoord0, refCoord1, refCoord2, refCoord3, predCount,
                inter_partition_mask, sad_adjustment, 
                cost_center0, cost_center1, cost_center2, cost_center3, 
                search_cost_precision, packed_cost_table,
                shape_penalty, ref_penalty,
                resultsout,
                accelerator );
        }

        // Perform IME for 4 different search areas based on neighbor predictors for ref15.

        if( num_use_refs > 0xF ) {
            resultsout = ime_streaminoutframe(
                src_img, refF0_img, srcCoord, 
                refCoord0, refCoord1, refCoord2, refCoord3, predCount,
                inter_partition_mask, sad_adjustment, 
                cost_center0, cost_center1, cost_center2, cost_center3, 
                search_cost_precision, packed_cost_table,
                shape_penalty, ref_penalty,
                resultsout,
                accelerator );
        }

        result = intel_sub_group_avc_ime_strip_single_reference_streamout( resultsout );

        // Get the best MVs, distortions and shapes across all previous IMEs.

        mvs          = intel_sub_group_avc_ime_get_motion_vectors( result );
        dists        = intel_sub_group_avc_ime_get_inter_distortions( result );
        best_dist    = intel_sub_group_avc_ime_get_best_inter_distortion( result );
        major_shape  = intel_sub_group_avc_ime_get_inter_major_shape( result );
        minor_shapes = intel_sub_group_avc_ime_get_inter_minor_shapes( result );
        shapes.s0    = major_shape;
        shapes.s1    = minor_shapes;
        directions   = intel_sub_group_avc_ime_get_inter_directions( result );
        ref_ids      = intel_sub_group_avc_ime_get_inter_reference_ids( result );

        // Perform FME for sub-pixel resolution results.
        
        ulong cost_center = cost_center0;
        intel_sub_group_avc_ref_payload_t payload_fme = intel_sub_group_avc_fme_initialize( srcCoord, mvs, major_shape, minor_shapes, directions, sub_pixel_mode, sad_adjustment );
        payload_fme = intel_sub_group_avc_ref_set_motion_vector_cost_function( cost_center, packed_cost_table, search_cost_precision, payload_fme );
        payload_fme = intel_sub_group_avc_ref_set_inter_base_multi_reference_penalty( ref_penalty, payload_fme );      
        payload_fme = intel_sub_group_avc_ref_set_inter_shape_penalty( shape_penalty, payload_fme );  

        intel_sub_group_avc_ref_result_t result_fme = intel_sub_group_avc_ref_evaluate_with_multi_reference( src_img, ref_ids, accelerator, payload_fme );

        dists = intel_sub_group_avc_ref_get_inter_distortions( result_fme );
        best_dist = intel_sub_group_avc_ref_get_best_inter_distortion( result_fme );
        mvs = intel_sub_group_avc_ref_get_motion_vectors( result_fme );

        if( ref_ids != intel_sub_group_avc_ref_get_inter_reference_ids( result_fme ) )
        {
	        printf("****************   ERROR 0 %d  *****************\n", ref_ids);
        }        
        
	    // Do a skip check to verify distortions with multi-ref penalties (just for validation or demo purposes).

	    if( ( major_shape == CLK_AVC_ME_MAJOR_16x16_INTEL ) || 
            ( major_shape == CLK_AVC_ME_MAJOR_8x8_INTEL && 
              minor_shapes == CLK_AVC_ME_MINOR_8x8_INTEL ) )
	    {
            uint skip_block_partition_type = 
                ( major_shape == CLK_AVC_ME_MAJOR_16x16_INTEL )? 
                 CLK_AVC_ME_SKIP_BLOCK_PARTITION_16x16_INTEL: CLK_AVC_ME_SKIP_BLOCK_PARTITION_8x8_INTEL;
            uint ids = get_sub_group_local_id() < 4 ? get_sub_group_local_id() * 4: 0;
		    long mvs_skc = intel_sub_group_shuffle( mvs, ids );
		    int2 mvs_skc_int = as_int2( mvs_skc );
		    mvs_skc = as_long( mvs_skc_int );
		    uint skip_motion_vector_mask = 
			    intel_sub_group_avc_sic_get_motion_vector_mask(
				    skip_block_partition_type,
				    directions );
		    uint bidirectional_weight = CLK_AVC_ME_BIDIR_WEIGHT_HALF_INTEL;
	
		    intel_sub_group_avc_sic_payload_t check_payload = intel_sub_group_avc_sic_initialize( srcCoord );         
		    check_payload = 
			    intel_sub_group_avc_sic_configure_skc(
				    skip_block_partition_type,
				    skip_motion_vector_mask,
				    mvs_skc,
				    bidirectional_weight,
				    sad_adjustment,
				    check_payload );

		    check_payload = intel_sub_group_avc_sic_set_motion_vector_cost_function( cost_center, packed_cost_table, search_cost_precision, check_payload );
		    check_payload = intel_sub_group_avc_sic_set_inter_base_multi_reference_penalty( ref_penalty, check_payload );
            check_payload = intel_sub_group_avc_sic_set_inter_shape_penalty( shape_penalty, check_payload );

		    intel_sub_group_avc_sic_result_t checked_result =
			    intel_sub_group_avc_sic_evaluate_with_multi_reference(
				    src_img,
				    ref_ids,
				    accelerator,
				    check_payload );

		    ushort checked_dists = intel_sub_group_avc_sic_get_inter_distortions( checked_result );

		    if( checked_dists != dists )
		    {
			    if( get_sub_group_local_id() == 0 ||  get_sub_group_local_id() == 4 )
			    {
				    printf(" *ERROR 2* : %d %d (%X : %d,%d :%d)\n", checked_dists, dists, skip_motion_vector_mask >> 24, mbid.x, mbid.y, get_sub_group_local_id());
			    }
		    }
	    }
    }

    //------------------- Perfrom intra estimation -------------------------
    
	uchar intra_partition_mask  = 
        CLK_AVC_ME_INTRA_LUMA_PARTITION_MASK_16x16_INTEL & CLK_AVC_ME_INTRA_LUMA_PARTITION_MASK_8x8_INTEL &
        CLK_AVC_ME_INTRA_LUMA_PARTITION_MASK_4x4_INTEL;
    uchar intra_edges =
        CLK_AVC_ME_INTRA_NEIGHBOR_LEFT_MASK_ENABLE_INTEL       |
        CLK_AVC_ME_INTRA_NEIGHBOR_UPPER_MASK_ENABLE_INTEL      |
        CLK_AVC_ME_INTRA_NEIGHBOR_UPPER_LEFT_MASK_ENABLE_INTEL |
        CLK_AVC_ME_INTRA_NEIGHBOR_UPPER_RIGHT_MASK_ENABLE_INTEL;

    // If this is a left-edge MB, then disable left edges.
    if( mbid.x == 0 ) {
        intra_edges &= ~CLK_AVC_ME_INTRA_NEIGHBOR_LEFT_MASK_ENABLE_INTEL;
        intra_edges &= ~CLK_AVC_ME_INTRA_NEIGHBOR_UPPER_LEFT_MASK_ENABLE_INTEL;
    }
    // If this is a right edge MB then disable right edges.
    if( mbid.x == get_num_groups(0) - 1 ) {
        intra_edges &= ~CLK_AVC_ME_INTRA_NEIGHBOR_UPPER_RIGHT_MASK_ENABLE_INTEL;
    }
    // If this is a top-edge MB, then disable top edges.
    if( mbid.y == 0 ) {
        intra_edges &= ~CLK_AVC_ME_INTRA_NEIGHBOR_UPPER_LEFT_MASK_ENABLE_INTEL;
        intra_edges &= ~CLK_AVC_ME_INTRA_NEIGHBOR_UPPER_RIGHT_MASK_ENABLE_INTEL;
        intra_edges &= ~CLK_AVC_ME_INTRA_NEIGHBOR_UPPER_MASK_ENABLE_INTEL;
    }

    int2 edge_coord = 0;
    uchar left_luma_edge = 0, left_upper_luma_pixel = 0, upper_luma_edge = 0, upper_right_luma_edge = 0;

    // Read left luma edge from luma plane.    
       
    edge_coord.x = srcCoord.x - 4;
    edge_coord.y = srcCoord.y;
    uint left_luma_edge_dw =
        intel_sub_group_media_block_read_ui(
            edge_coord,
            1,
            16,
            src_luma_img );
    left_luma_edge = as_uchar4( left_luma_edge_dw ).s3;   

    // Read upper left luma corner from luma plane.
   
    edge_coord.x = srcCoord.x - 4;
    edge_coord.y = srcCoord.y - 1;
    uint left_upper_luma_pixel_dw =
        intel_sub_group_media_block_read_ui(
            edge_coord,
            1,
            16,
            src_luma_img );
    left_upper_luma_pixel = as_uchar4( left_upper_luma_pixel_dw ).s3;
    left_upper_luma_pixel = intel_sub_group_shuffle( left_upper_luma_pixel, 0 );
    
    // Read upper luma edge from luma plane.

    edge_coord.x = srcCoord.x;
    edge_coord.y = srcCoord.y - 1;
    upper_luma_edge =
        intel_sub_group_media_block_read_uc(
            edge_coord,
            16,
            1,
            src_luma_img );

    // Read upper right luma edge from luma plane.

    edge_coord.x = srcCoord.x + 16;
    edge_coord.y = srcCoord.y - 1;
    upper_right_luma_edge =
        intel_sub_group_media_block_read_uc(
            edge_coord,
            16,
            1,
            src_luma_img );

    // Read left neighbor modes.

    bool do_mode_costing = false;

    bool do_left_mode_costing = false;
    bool do_top_mode_costing = false;

    ulong left_modes = 0, top_modes = 0;
    uchar left_shape = 0, top_shape = 0;

    if( intra_edges & CLK_AVC_ME_INTRA_NEIGHBOR_LEFT_MASK_ENABLE_INTEL ) {
        left_modes = intra_luma_modes[ (mbid.x - 1) + mbid.y * ngrp.x ];
        left_shape = intra_luma_shape[ (mbid.x - 1) + mbid.y * ngrp.x ];
        do_left_mode_costing = true;
    }

    // Read top neighbor modes and shape.

    if( intra_edges & CLK_AVC_ME_INTRA_NEIGHBOR_UPPER_MASK_ENABLE_INTEL ) {
        top_modes = intra_luma_modes[ mbid.x + (mbid.y - 1) * ngrp.x ];
        top_shape = intra_luma_shape[ mbid.x + (mbid.y - 1) * ngrp.x ];
        do_top_mode_costing = true;
    }

    do_mode_costing = do_left_mode_costing & do_top_mode_costing;
   
    uint neighbor_luma_modes = 0;

    // Compose neighbor modes from left neighbor.

    if( do_mode_costing ) {
        if( left_shape == CLK_AVC_ME_INTRA_16x16_INTEL ) {
            neighbor_luma_modes |= (left_modes & 0xF) << 0;
            neighbor_luma_modes |= (left_modes & 0xF) << 4;
            neighbor_luma_modes |= (left_modes & 0xF) << 8;
            neighbor_luma_modes |= (left_modes & 0xF) << 12;

            if( (left_modes & 0xF) > 3 ) {
                printf("ERROR 3: Invalid intra neighbor mode %d for (%d, %d)\n", left_modes & 0xF, mbid.x, mbid.y);
            }
        }
        else if( left_shape == CLK_AVC_ME_INTRA_8x8_INTEL ) {
            neighbor_luma_modes |= ((left_modes >> 16) & 0xF) << 0;
            neighbor_luma_modes |= ((left_modes >> 16) & 0xF) << 4;
            neighbor_luma_modes |= ((left_modes >> 48) & 0xF) << 8;
            neighbor_luma_modes |= ((left_modes >> 48) & 0xF) << 12;

            if( ((left_modes >> 16) & 0xF) > 8 ) {
                printf("ERROR 4: Invalid intra neighbor mode %d for (%d, %d)\n", (left_modes >> 16) & 0xF, mbid.x, mbid.y);
            }
            if( ((left_modes >> 48) & 0xF) > 8 ) {
                printf("ERROR 4: Invalid intra neighbor mode %d for (%d, %d)\n", (left_modes >> 48) & 0xF, mbid.x, mbid.y);
            }
        }
        else {
            neighbor_luma_modes |= ((left_modes >> 20) & 0xF) << 0;
            neighbor_luma_modes |= ((left_modes >> 28) & 0xF) << 4;
            neighbor_luma_modes |= ((left_modes >> 52) & 0xF) << 8;
            neighbor_luma_modes |= ((left_modes >> 60) & 0xF) << 12;

            if( ((left_modes >> 20) & 0xF) > 8 ) {
                printf("ERROR 5: Invalid intra neighbor mode %d for (%d, %d)\n", (left_modes >> 20) & 0xF, mbid.x, mbid.y);
            }
            if( ((left_modes >> 28) & 0xF) > 8 ) {
                printf("ERROR 5: Invalid intra neighbor mode %d for (%d, %d)\n", (left_modes >> 28) & 0xF, mbid.x, mbid.y);
            }
            if( ((left_modes >> 52) & 0xF) > 8 ) {
                printf("ERROR 5: Invalid intra neighbor mode %d for (%d, %d)\n", (left_modes >> 52) & 0xF, mbid.x, mbid.y);
            }
            if( ((left_modes >> 60) & 0xF) > 8 ) {
                printf("ERROR 5: Invalid intra neighbor mode %d for (%d, %d)\n", (left_modes >> 60) & 0xF, mbid.x, mbid.y);
            }
        }
    }

    // Compose neighbor modes from top neighbor.

    if( do_mode_costing ) {
        if( top_shape == CLK_AVC_ME_INTRA_16x16_INTEL ) {
            neighbor_luma_modes |= (top_modes & 0xF) << 0;
            neighbor_luma_modes |= (top_modes & 0xF) << 4;
            neighbor_luma_modes |= (top_modes & 0xF) << 8;
            neighbor_luma_modes |= (top_modes & 0xF) << 12;

            if( (top_modes & 0xF) > 3 ) {
                printf("ERROR 6: Invalid intra neighbor mode %d for (%d, %d)\n", top_modes & 0xF, mbid.x, mbid.y);
            }
            if( (top_modes & 0xF) > 3 ) {
                printf("ERROR 6: Invalid intra neighbor mode %d for (%d, %d)\n", top_modes & 0xF, mbid.x, mbid.y);
            }
        }
        else if( top_shape == CLK_AVC_ME_INTRA_8x8_INTEL ) {
            neighbor_luma_modes |= ((top_modes >> 32) & 0xF) << 16;
            neighbor_luma_modes |= ((top_modes >> 32) & 0xF) << 20;
            neighbor_luma_modes |= ((top_modes >> 48) & 0xF) << 24;
            neighbor_luma_modes |= ((top_modes >> 48) & 0xF) << 28;

            if( ((top_modes >> 32) & 0xF) > 8 ) {
                printf("ERROR 7: Invalid intra neighbor mode %d for (%d, %d)\n", (top_modes >> 32) & 0xF, mbid.x, mbid.y);
            }
            if( ((top_modes >> 48) & 0xF) > 8 ) {
                printf("ERROR 7: Invalid intra neighbor mode %d for (%d, %d)\n", (top_modes >> 48) & 0xF, mbid.x, mbid.y);
            }
        }
        else {
            neighbor_luma_modes |= ((top_modes >> 40) & 0xFF) << 16;
            neighbor_luma_modes |= ((top_modes >> 56) & 0xFF) << 24;

            if( ((top_modes >> 40) & 0xF) > 8 ) {
                printf("ERROR 8: Invalid intra neighbor mode %d for (%d, %d)\n", (top_modes >> 40) & 0xF, mbid.x, mbid.y);
            }
            if( ((top_modes >> 44) & 0xF) > 8 ) {
                printf("ERROR 8: Invalid intra neighbor mode %d for (%d, %d)\n", (top_modes >> 44) & 0xF, mbid.x, mbid.y);
            }
            if( ((top_modes >> 56) & 0xF) > 8 ) {
                printf("ERROR 8: Invalid intra neighbor mode %d for (%d, %d)\n", (top_modes >> 56) & 0xF, mbid.x, mbid.y);
            }
            if( ((top_modes >> 60) & 0xF) > 8 ) {
                printf("ERROR 8: Invalid intra neighbor mode %d for (%d, %d)\n", (top_modes >> 60) & 0xF, mbid.x, mbid.y);
            }
        }
    }

    intel_sub_group_avc_sic_payload_t sic_payload;
    sic_payload = intel_sub_group_avc_sic_initialize( srcCoord );
    sic_payload = 
        intel_sub_group_avc_sic_configure_ipe(
            intra_partition_mask, intra_edges,
            left_luma_edge, left_upper_luma_pixel, upper_luma_edge, upper_right_luma_edge,
            sad_adjustment, sic_payload );

    uint intra_luma_shape_penalty = intel_sub_group_avc_mce_get_default_intra_luma_shape_penalty( slice_type, qp );
    uchar intra_luma_mode_base_penalty = intel_sub_group_avc_mce_get_default_intra_luma_mode_penalty( slice_type, qp );
    uint intra_luma_packed_non_dc_penalty = intel_sub_group_avc_mce_get_default_non_dc_luma_intra_penalty();

    intra_luma_shape_penalty = intel_sub_group_avc_mce_get_default_intra_luma_shape_penalty( slice_type, qp );
    sic_payload = intel_sub_group_avc_sic_set_intra_luma_shape_penalty( intra_luma_shape_penalty, sic_payload );

    if( do_mode_costing ) {
        sic_payload = intel_sub_group_avc_sic_set_intra_luma_mode_cost_function(
            intra_luma_mode_base_penalty, neighbor_luma_modes, intra_luma_packed_non_dc_penalty, sic_payload );
    }

    intel_sub_group_avc_sic_result_t ipe_result;
    ipe_result = intel_sub_group_avc_sic_evaluate_ipe( src_img, accelerator, sic_payload );

    uchar intra_shape = intel_sub_group_avc_sic_get_ipe_luma_shape( ipe_result );
    ushort intra_distortions = intel_sub_group_avc_sic_get_best_ipe_luma_distortion( ipe_result );
    ulong intra_modes = intel_sub_group_avc_sic_get_packed_ipe_luma_modes( ipe_result );
   
    // --------------------- Write out results -------------------------

    if( num_use_refs ) {
        int index = ( mbid.x * 16 + get_local_id(0) ) + ( mbid.y * 16 * ngrp.x );
        int2 bi_mvs = as_int2( mvs );
        motion_vector_buffer [ index ] = as_short2( bi_mvs.s0 );
        residuals_buffer[ index ] = dists;
        best_residual_buffer[ mbid.x + mbid.y * ngrp.x ] = best_dist;
        shapes_buffer[ mbid.x + mbid.y * ngrp.x ] = shapes;
        reference_id_buffer[ mbid.x + mbid.y * ngrp.x ] = ref_ids;
    }

    intra_luma_shape[ mbid.x + mbid.y * ngrp.x ] = intra_shape;
    intra_luma_residuals[ mbid.x + mbid.y * ngrp.x ] = intra_distortions;
    intra_luma_modes[ mbid.x + mbid.y * ngrp.x ] = intra_modes;
	
	// The acquire-release semantics of the atomic operations used for the poll 
	// and signal operation will ensure that the writes are visible to other
	// threads prior to the scoreboard  updates becoming visible.

    // ....................Signaling phase.................................	
	if( get_sub_group_local_id() == 0 ) 
	{
		signal_scoreboard(mbid, ngrp, scoreboard);
	}
}
