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
    payload = intel_sub_group_avc_ime_set_inter_shape_penalty( shape_penalty, payload );
    resultsout = intel_sub_group_avc_ime_evaluate_with_single_reference_streaminout( src_img, ref_img, accelerator, payload, resultsin );
    return resultsout;
}

inline intel_sub_group_avc_ime_result_single_reference_streamout_t 
ime_streamoutframe(
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
    sampler_t accelerator )
{
    intel_sub_group_avc_ime_result_single_reference_streamout_t resultsout;
    short2 zero_ref_coord = 0;
    ulong zero_cost_center = 0;
    resultsout = ime_streamout(
        src_img, ref_img, src_coord, zero_ref_coord, partition_mask, sad_adjustment, 
        zero_cost_center, search_cost_precision, packed_cost_table,
        shape_penalty, 
        accelerator );
    resultsout = ime_streaminout(
        src_img, ref_img, src_coord, 
        ref_coord, 
        partition_mask, sad_adjustment, 
        cost_center, search_cost_precision, packed_cost_table,
        shape_penalty, 
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
    __read_only image2d_t   src_img,
    __read_only image2d_t   ref_img, 

     __global short2*       predictor_buffer,

    __global short2*        motion_vector_buffer,
    __global uchar2*        shapes_buffer ) 
{
    int2 gid = { get_group_id(0), get_group_id(1) };
    int2 ngrp = { get_num_groups(0), get_num_groups(1) };   
    
    int2 mbid = gid;
    int lid = gid.y * ngrp.x + gid.x;          

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
    uchar major_shape  = 0;
    uchar minor_shapes = 0;
    uchar2 shapes      = 0;
    uchar directions   = 0;

    //------------------- Perfrom inter estimation -------------------------
  
    short2 predMV0 = predictor_buffer [ mbid.x + mbid.y * ngrp.x ];

    short2 refCoord0;        

    srcCoord.x = mbid.x * 16;
    srcCoord.y = mbid.y * 16;

    // Obtain refCoord in PEL resolution and cost center in QPEL resolution.

    refCoord0 = predMV0 >> 2; 

    // Obtain cost centers.

    ulong cost_center0;       
        
    cost_center0 = get_cost_center(0);

    uint2 packed_cost_table = intel_sub_group_avc_mce_get_default_high_penalty_cost_table();    

    ulong shape_penalty = intel_sub_group_avc_mce_get_default_inter_shape_penalty( slice_type, qp );

    intel_sub_group_avc_ime_result_single_reference_streamout_t resultsout;
    intel_sub_group_avc_ime_result_t result;
    
    resultsout = ime_streamoutframe(
        src_img, ref_img, srcCoord, 
        refCoord0,
        inter_partition_mask, sad_adjustment, 
        cost_center0,
        search_cost_precision, packed_cost_table,
        shape_penalty, 
        accelerator );

    result = intel_sub_group_avc_ime_strip_single_reference_streamout( resultsout );

    // Get the best MVs, distortions and shapes across all previous IMEs.

    mvs          = intel_sub_group_avc_ime_get_motion_vectors( result );
    major_shape  = intel_sub_group_avc_ime_get_inter_major_shape( result );
    minor_shapes = intel_sub_group_avc_ime_get_inter_minor_shapes( result );
    shapes.s0    = major_shape;
    shapes.s1    = minor_shapes;
    directions   = intel_sub_group_avc_ime_get_inter_directions( result );

    // Perform FME for sub-pixel resolution results.
        
    ulong cost_center = cost_center0;
    intel_sub_group_avc_ref_payload_t payload_fme = intel_sub_group_avc_fme_initialize( srcCoord, mvs, major_shape, minor_shapes, directions, sub_pixel_mode, sad_adjustment );
    payload_fme = intel_sub_group_avc_ref_set_motion_vector_cost_function( cost_center, packed_cost_table, search_cost_precision, payload_fme ); 
    payload_fme = intel_sub_group_avc_ref_set_inter_shape_penalty( shape_penalty, payload_fme );  

    intel_sub_group_avc_ref_result_t result_fme = intel_sub_group_avc_ref_evaluate_with_single_reference( src_img, ref_img, accelerator, payload_fme );

    mvs = intel_sub_group_avc_ref_get_motion_vectors( result_fme );      
   
    // --------------------- Write out results -------------------------

    int index = ( mbid.x * 16 + get_local_id(0) ) + ( mbid.y * 16 * ngrp.x );
    int2 bi_mvs = as_int2( mvs );
    motion_vector_buffer [ index ] = as_short2( bi_mvs.s0 );
    shapes_buffer[ mbid.x + mbid.y * ngrp.x ] = shapes;
}
