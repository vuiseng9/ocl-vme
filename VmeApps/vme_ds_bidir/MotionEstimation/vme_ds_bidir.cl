/*************************************************************************************************\
 Kernel: block_motion_estimate_bidir_intel


 Description: Perform bidirectional motion estimation using one source frame and
one forward and one backward reference frame. Initial
 predictor motion vectors can be passed as arguments. If no skip check is to be
performed, this kernel allows all major and minor shapes.
 If skip check is enabled, only 8x8 or 16x16 partition sizes are enabled,
depending on the skip check setting.

 Returns:  (i)   Best motion vectors found, which may be forward, backward or
                 bidirectional, in search_motion_vector_buffer
                 This buffer has 16 motion vectors per 16x16 macroblock. Valid
                 motion vectors are filled according to the major and minor shape
                 and direction found
           (ii)  The best partitioning of each 16x16 macroblock into major and
                 minor block sizes in shapes_buffer.
           (iii) The best prediction direction (forward, backward or
                 bidirectional) for each partition indicated in the shapes_buffer.
           (iv)  The residual error corresponding to each block size and motion
                 vector in search_residuals buffer.

\*************************************************************************************************/

__kernel __attribute__((reqd_work_group_size(16, 1, 1))) void
block_motion_estimate_bidir_intel(
    __read_only image2d_t srcImg, 
    __read_only image2d_t fwRefImg,
    __read_only image2d_t bwRefImg, 
    uint cost_penalty, 
    uint cost_precision,
    __global short2 *fwd_prediction_motion_vector_buffer,
    __global short2 *bwd_prediction_motion_vector_buffer,
    __global uint2 *search_motion_vector_buffer,
    __global ushort *search_residuals, 
    __global uchar2 *shapes_buffer,
    __global uchar *directions_buffer, 
    int iterations, 
    int skp_check_type) 
{
  int gid_0 = get_group_id(0); // mb col
  int gid_1 = 0;               // mb row

  sampler_t accelerator = 0;

  for (int i = 0; i < iterations; i++, gid_1++) {
    ushort2 srcCoord;

    srcCoord.x = gid_0 * 16 + get_global_offset(0);
    srcCoord.y = gid_1 * 16 + get_global_offset(1);

    uint curMB = gid_0 + gid_1 * get_num_groups(0);

    uchar partition_mask = CLK_AVC_ME_PARTITION_MASK_ALL_INTEL;

    if (skp_check_type == 1)
      partition_mask = CLK_AVC_ME_PARTITION_MASK_8x8_INTEL;
    if (skp_check_type == 2)
      partition_mask = CLK_AVC_ME_PARTITION_MASK_16x16_INTEL;

    short2 fwRefCoord = 0;
    short2 bwRefCoord = 0;
    short2 predMV = 0;

    if (fwd_prediction_motion_vector_buffer != NULL) {
      predMV = fwd_prediction_motion_vector_buffer[curMB];

      fwRefCoord.x = predMV.x / 4;
      fwRefCoord.y = predMV.y / 4;
      fwRefCoord.y = fwRefCoord.y & 0xFFFE;
    }

    if (bwd_prediction_motion_vector_buffer != NULL) {
      predMV = bwd_prediction_motion_vector_buffer[curMB];

      bwRefCoord.x = predMV.x / 4;
      bwRefCoord.y = predMV.y / 4;
      bwRefCoord.y = bwRefCoord.y & 0xFFFE;
    }

    uchar sad_adjustment = CLK_AVC_ME_SAD_ADJUST_MODE_NONE_INTEL;
    uchar pixel_mode = CLK_AVC_ME_SUBPIXEL_MODE_QPEL_INTEL;

    intel_sub_group_avc_ime_payload_t payload =
        intel_sub_group_avc_ime_initialize(srcCoord, partition_mask,
                                           sad_adjustment);

    payload = intel_sub_group_avc_ime_set_dual_reference(
        fwRefCoord, bwRefCoord, CLK_AVC_ME_SEARCH_WINDOW_EXHAUSTIVE_INTEL,
        payload);

    uint2 packed_cost_table = 0;

    if (cost_penalty == 1)
      packed_cost_table =
          intel_sub_group_avc_mce_get_default_low_penalty_cost_table();
    else if (cost_penalty == 2)
      packed_cost_table =
          intel_sub_group_avc_mce_get_default_medium_penalty_cost_table();
    else if (cost_penalty == 3)
      packed_cost_table =
          intel_sub_group_avc_mce_get_default_high_penalty_cost_table();

    // Cost coords are in QPEL resolution.

    ulong packed_cost_center_delta = 0;

    payload = intel_sub_group_avc_ime_set_motion_vector_cost_function(
        packed_cost_center_delta, packed_cost_table, cost_precision, payload);

    intel_sub_group_avc_ime_result_t result0 =
        intel_sub_group_avc_ime_evaluate_with_dual_reference(
            srcImg, fwRefImg, bwRefImg, accelerator, payload);

    uchar major_shape = intel_sub_group_avc_ime_get_inter_major_shape(result0);
    uchar minor_shapes =
        intel_sub_group_avc_ime_get_inter_minor_shapes(result0);
    uchar2 shapes = {major_shape, minor_shapes};
    uchar directions = intel_sub_group_avc_ime_get_inter_directions(result0);

    long mvs_ime = intel_sub_group_avc_ime_get_motion_vectors(result0);
    ushort dist_ime = intel_sub_group_avc_ime_get_inter_distortions(result0);

    long mvs;
    ushort dist;
    uchar bidir_weight = CLK_AVC_ME_BIDIR_WEIGHT_HALF_INTEL;

    if (pixel_mode != CLK_AVC_ME_SUBPIXEL_MODE_INTEGER_INTEL) {
      intel_sub_group_avc_ref_payload_t payload =
          intel_sub_group_avc_bme_initialize(
              srcCoord, mvs_ime, major_shape, minor_shapes, directions,
              pixel_mode, bidir_weight, sad_adjustment);

      uint2 packed_cost_table = 0;
      if (cost_penalty == 1)
        packed_cost_table =
            intel_sub_group_avc_mce_get_default_low_penalty_cost_table();
      else if (cost_penalty == 2)
        packed_cost_table =
            intel_sub_group_avc_mce_get_default_medium_penalty_cost_table();
      else if (cost_penalty == 3)
        packed_cost_table =
            intel_sub_group_avc_mce_get_default_high_penalty_cost_table();

      ulong packed_cost_center_delta = 0;

      payload = intel_sub_group_avc_ref_set_motion_vector_cost_function(
          packed_cost_center_delta, packed_cost_table, cost_precision, payload);

      intel_sub_group_avc_ref_result_t result =
          intel_sub_group_avc_ref_evaluate_with_dual_reference(
              srcImg, fwRefImg, bwRefImg, accelerator, payload);

      mvs = intel_sub_group_avc_ref_get_motion_vectors(result);
      dist = intel_sub_group_avc_ref_get_inter_distortions(result);
      directions = intel_sub_group_avc_ref_get_inter_directions(result);
    } else {
      mvs = mvs_ime;
      dist = dist_ime;
    }

    uint2 bi_mvs = as_uint2(mvs);

    int index = (gid_0 * 16 + get_local_id(0)) +
                (gid_1 * 16 * get_num_groups(0)); // assumes 16 MV per MB

    search_motion_vector_buffer[index] = bi_mvs;

    if (search_residuals != NULL)
      search_residuals[index] = dist;

    shapes_buffer[curMB] = shapes;
    directions_buffer[curMB] = directions;
  }
}

/*************************************************************************************************\
 Kernel: block_motion_estimate_fwd_intel


 Description: Perform forward (unidirectional) motion estimation using one
source frame and one forward reference frame. Initial
 predictor motion vectors can be passed as arguments. If no skip check is to be
performed, this kernel allows all major and minor shapes.
 If skip check is enabled, only 8x8 or 16x16 partition sizes are enabled,
depending on the skip check setting.

 Returns:  (i)   Best motion vectors found, in search_motion_vector_buffer
                 This buffer has 16 motion vectors per 16x16 macroblock. Valid
                 motion vectors are filled according to the major and minor shape.
           (ii)  The best partitioning of each 16x16 macroblock into major and
                 minor block sizes in shapes_buffer.
           (iii) The residual error corresponding to each block size and motion
                 vector in search_residuals buffer.

           The directions_buffer is also filled (with all directions set to
           forward) to allow reuse of the motion vector overlay code.

\*************************************************************************************************/

__kernel __attribute__((reqd_work_group_size(16, 1, 1))) void
block_motion_estimate_fwd_intel(
    __read_only image2d_t srcImg, 
    __read_only image2d_t fwRefImg,
    uint cost_penalty, 
    uint cost_precision,
    __global short2 *fwd_prediction_motion_vector_buffer,
    __global uint2 *search_motion_vector_buffer,
    __global ushort *search_residuals, 
    __global uchar2 *shapes_buffer,
    __global uchar *directions_buffer, 
    int iterations, 
    int skp_check_type) 
{
  int gid_0 = get_group_id(0); // mb col
  int gid_1 = 0;               // mb row

  sampler_t accelerator = 0;

  for (int i = 0; i < iterations; i++, gid_1++) {
    ushort2 srcCoord;

    srcCoord.x = gid_0 * 16 + get_global_offset(0);
    srcCoord.y = gid_1 * 16 + get_global_offset(1);

    uint curMB =
        gid_0 + gid_1 * get_num_groups(0); // current MB index in raster order

    uchar partition_mask = CLK_AVC_ME_PARTITION_MASK_ALL_INTEL;

    if (skp_check_type == 1)
      partition_mask = CLK_AVC_ME_PARTITION_MASK_8x8_INTEL;
    if (skp_check_type == 2)
      partition_mask = CLK_AVC_ME_PARTITION_MASK_16x16_INTEL;

    short2 fwRefCoord = 0;

    short2 predMV = 0;

    if (fwd_prediction_motion_vector_buffer != NULL) {
      predMV = fwd_prediction_motion_vector_buffer[curMB];

      fwRefCoord.x = predMV.x / 4;
      fwRefCoord.y = predMV.y / 4;
      fwRefCoord.y = fwRefCoord.y & 0xFFFE;
    }

    uchar sad_adjustment = CLK_AVC_ME_SAD_ADJUST_MODE_NONE_INTEL;
    uchar pixel_mode = CLK_AVC_ME_SUBPIXEL_MODE_QPEL_INTEL;

    intel_sub_group_avc_ime_payload_t payload =
        intel_sub_group_avc_ime_initialize(srcCoord, partition_mask,
                                           sad_adjustment);

    payload = intel_sub_group_avc_ime_set_single_reference(
        fwRefCoord, CLK_AVC_ME_SEARCH_WINDOW_EXHAUSTIVE_INTEL, payload);

    uint2 packed_cost_table = 0;

    if (cost_penalty == 1)
      packed_cost_table =
          intel_sub_group_avc_mce_get_default_low_penalty_cost_table();
    else if (cost_penalty == 2)
      packed_cost_table =
          intel_sub_group_avc_mce_get_default_medium_penalty_cost_table();
    else if (cost_penalty == 3)
      packed_cost_table =
          intel_sub_group_avc_mce_get_default_high_penalty_cost_table();

    // Cost coords are in QPEL resolution.

    ulong packed_cost_center_delta = 0;

    payload = intel_sub_group_avc_ime_set_motion_vector_cost_function(
        packed_cost_center_delta, packed_cost_table, cost_precision, payload);

    intel_sub_group_avc_ime_result_t result0 =
        intel_sub_group_avc_ime_evaluate_with_single_reference(
            srcImg, fwRefImg, accelerator, payload);

    uchar major_shape = intel_sub_group_avc_ime_get_inter_major_shape(result0);
    uchar minor_shapes =
        intel_sub_group_avc_ime_get_inter_minor_shapes(result0);
    uchar2 shapes = {major_shape, minor_shapes};
    uchar directions = intel_sub_group_avc_ime_get_inter_directions(result0);

    long mvs_ime = intel_sub_group_avc_ime_get_motion_vectors(result0);
    ushort dist_ime = intel_sub_group_avc_ime_get_inter_distortions(result0);

    long mvs;
    ushort dist;

    if (pixel_mode != CLK_AVC_ME_SUBPIXEL_MODE_INTEGER_INTEL) {
      intel_sub_group_avc_ref_payload_t payload =
          intel_sub_group_avc_fme_initialize(srcCoord, mvs_ime, major_shape,
                                             minor_shapes, directions,
                                             pixel_mode, sad_adjustment);

      uint2 packed_cost_table = 0;
      if (cost_penalty == 1)
        packed_cost_table =
            intel_sub_group_avc_mce_get_default_low_penalty_cost_table();
      else if (cost_penalty == 2)
        packed_cost_table =
            intel_sub_group_avc_mce_get_default_medium_penalty_cost_table();
      else if (cost_penalty == 3)
        packed_cost_table =
            intel_sub_group_avc_mce_get_default_high_penalty_cost_table();

      ulong packed_cost_center_delta = 0;

      payload = intel_sub_group_avc_ref_set_motion_vector_cost_function(
          packed_cost_center_delta, packed_cost_table, cost_precision, payload);

      intel_sub_group_avc_ref_result_t result =
          intel_sub_group_avc_ref_evaluate_with_single_reference(
              srcImg, fwRefImg, accelerator, payload);

      mvs = intel_sub_group_avc_ref_get_motion_vectors(result);
      dist = intel_sub_group_avc_ref_get_inter_distortions(result);
      directions = intel_sub_group_avc_ref_get_inter_directions(result);

    } else {
      mvs = mvs_ime;
      dist = dist_ime;
    }

    uint2 bi_mvs = as_uint2(mvs);

    int index = (gid_0 * 16 + get_local_id(0)) +
                (gid_1 * 16 * get_num_groups(0)); // assumes 16 MV per MB

    search_motion_vector_buffer[index] = bi_mvs;

    if (search_residuals != NULL) {
      search_residuals[index] = dist;
    }

    shapes_buffer[curMB] = shapes;
    directions_buffer[curMB] = directions;
  }
}

/*************************************************************************************************\
 Kernel: block_skip_check_bidir_intel

 Description: This kernel takes as input forward, backward or bidirectional
              motion vectors, partitioning of each MB into major and
              minor shape, and motion vector directions. It uses two reference frames and one
              source frame to determine the residual error if
              the given motion vectors are used to do motion estimation for the given frames.

 Returns: Residual error in skip_residuals buffer.

\*************************************************************************************************/

__kernel __attribute__((reqd_work_group_size(16, 1, 1))) void
block_skip_check_bidir_intel(
    __read_only image2d_t srcImg,
    __read_only image2d_t fwRefImg,
    __read_only image2d_t bwRefImg,
    uint skip_block_type, 
    uint cost_penalty,
    uint cost_precision,
    __global uint2 *bidir_motion_vector_buffer,
    __global uchar *dir_buffer,
    __global ushort *skip_residuals, 
    int iterations) 
{
  sampler_t accelerator = 0;

  int gid_0 = get_group_id(0);
  int gid_1 = 0;

  for (int i = 0; i < iterations; i++, gid_1++) {

    ushort2 srcCoord;

    srcCoord.x = gid_0 * 16 +
                 get_global_offset(0); // 16 pixels wide MBs (globally scalar)
    srcCoord.y = gid_1 * 16 +
                 get_global_offset(1); // 16 pixels tall MBs (globally scalar)

    uint curMB = gid_0 + gid_1 * get_num_groups(0); // current MB id

    uchar sad_adjustment = CLK_AVC_ME_SAD_ADJUST_MODE_NONE_INTEL;
    uchar pixel_mode = CLK_AVC_ME_SUBPIXEL_MODE_QPEL_INTEL;

    uint skip_block_partition_type = 0;
    uint skip_motion_vector_mask = 0;

    uint2 bidir_mv;

    if (skip_block_type == 0x0) {
      bidir_mv = bidir_motion_vector_buffer[curMB];

      skip_block_partition_type = CLK_AVC_ME_SKIP_BLOCK_PARTITION_16x16_INTEL;

      uchar dir = dir_buffer[curMB] & 0x03;
      skip_motion_vector_mask = intel_sub_group_avc_sic_get_motion_vector_mask(
          skip_block_partition_type, dir_buffer[curMB]);
    }

    else if (skip_block_type == 0x1) {
      uint offset = get_sub_group_local_id() % 4; // work-items 0,1,2,3 in subgroup
                                                  // get corresponding bidir_mv,
                                                  // remaining work-items not
                                                  // relevant
      bidir_mv = bidir_motion_vector_buffer[curMB * 4 + offset];

      skip_block_partition_type = CLK_AVC_ME_SKIP_BLOCK_PARTITION_8x8_INTEL;
      skip_motion_vector_mask = intel_sub_group_avc_sic_get_motion_vector_mask(
          skip_block_partition_type, dir_buffer[curMB]);
    }

    long mv = as_long(bidir_mv); // bidir MV

    intel_sub_group_avc_sic_payload_t payload =
        intel_sub_group_avc_sic_initialize(srcCoord);

    uchar bidir_weight = CLK_AVC_ME_BIDIR_WEIGHT_HALF_INTEL;

    payload = intel_sub_group_avc_sic_configure_skc(
        skip_block_partition_type, skip_motion_vector_mask, mv, bidir_weight,
        sad_adjustment, payload);

    // set cost penalty
    uint2 packed_cost_table = 0;

    if (cost_penalty == 1)
      packed_cost_table =
          intel_sub_group_avc_mce_get_default_low_penalty_cost_table();
    else if (cost_penalty == 2)
      packed_cost_table =
          intel_sub_group_avc_mce_get_default_medium_penalty_cost_table();
    else if (cost_penalty == 3)
      packed_cost_table =
          intel_sub_group_avc_mce_get_default_high_penalty_cost_table();

    ulong packed_cost_center_delta = 0;

    payload = intel_sub_group_avc_sic_set_motion_vector_cost_function(
        packed_cost_center_delta, packed_cost_table, cost_precision, payload);

    intel_sub_group_avc_sic_result_t result =
        intel_sub_group_avc_sic_evaluate_with_dual_reference(
            srcImg, fwRefImg, bwRefImg, accelerator, payload);

    ushort dists = intel_sub_group_avc_sic_get_inter_distortions(result);

    int index = (gid_0 * 16 + get_local_id(0)) +
                (gid_1 * 16 * get_num_groups(0)); // assumes 16 MV per MB

    if (skip_residuals != NULL)
      skip_residuals[index] = dists;
  }
}

/*************************************************************************************************\
 Kernel: block_skip_check_fwd_intel

 Description: This kernel takes as input forward motion vectors (still using a
              uint2 format for simplicity).
              It uses one reference frame and one source frame to determine the residual
              error if the given motion vectors are used to do motion estimation for the given 
              frame.

 Returns: Residual error in skip_residuals buffer.

\*************************************************************************************************/

__kernel __attribute__((reqd_work_group_size(16, 1, 1))) void
block_skip_check_fwd_intel(
    __read_only image2d_t srcImg,
    __read_only image2d_t fwRefImg, 
    uint skip_block_type,
    uint cost_penalty, 
    uint cost_precision,
    __global uint2 *bidir_motion_vector_buffer,
    __global ushort *skip_residuals, 
    int iterations)
{
  sampler_t accelerator = 0;

  int gid_0 = get_group_id(0);
  int gid_1 = 0;

  for (int i = 0; i < iterations; i++, gid_1++) {

    ushort2 srcCoord;

    srcCoord.x = gid_0 * 16 +
                 get_global_offset(0); // 16 pixels wide MBs (globally scalar)
    srcCoord.y = gid_1 * 16 +
                 get_global_offset(1); // 16 pixels tall MBs (globally scalar)

    uint curMB = gid_0 + gid_1 * get_num_groups(0); // current MB id

    uchar sad_adjustment = CLK_AVC_ME_SAD_ADJUST_MODE_NONE_INTEL;
    uchar pixel_mode = CLK_AVC_ME_SUBPIXEL_MODE_QPEL_INTEL;

    uint skip_block_partition_type = 0;
    uint skip_motion_vector_mask = 0;

    uint2 bidir_mv;

    if (skip_block_type == 0x0) {
      bidir_mv = bidir_motion_vector_buffer[curMB];

      skip_block_partition_type = CLK_AVC_ME_SKIP_BLOCK_PARTITION_16x16_INTEL;
      skip_motion_vector_mask =
          CLK_AVC_ME_SKIP_BLOCK_16x16_FORWARD_ENABLE_INTEL;
    }

    else if (skip_block_type == 0x1) {
      uint offset = get_sub_group_local_id() % 4; // work-items 0,1,2,3 in subgroup
                                                  // get corresponding bidir_mv,
                                                  // remaining work-items not
                                                  // relevant
      bidir_mv = bidir_motion_vector_buffer[curMB * 4 + offset];

      skip_block_partition_type = CLK_AVC_ME_SKIP_BLOCK_PARTITION_8x8_INTEL;
      skip_motion_vector_mask = CLK_AVC_ME_SKIP_BLOCK_8x8_FORWARD_ENABLE_INTEL;
    }

    long mv = as_long(bidir_mv); // bidir MV

    intel_sub_group_avc_sic_payload_t payload =
        intel_sub_group_avc_sic_initialize(srcCoord);

    uchar bidir_weight = CLK_AVC_ME_BIDIR_WEIGHT_HALF_INTEL;

    payload = intel_sub_group_avc_sic_configure_skc(
        skip_block_partition_type, skip_motion_vector_mask, mv, bidir_weight,
        sad_adjustment, payload);

    // set cost penalty
    uint2 packed_cost_table = 0;

    if (cost_penalty == 1)
      packed_cost_table =
          intel_sub_group_avc_mce_get_default_low_penalty_cost_table();
    else if (cost_penalty == 2)
      packed_cost_table =
          intel_sub_group_avc_mce_get_default_medium_penalty_cost_table();
    else if (cost_penalty == 3)
      packed_cost_table =
          intel_sub_group_avc_mce_get_default_high_penalty_cost_table();

    ulong packed_cost_center_delta = 0;

    payload = intel_sub_group_avc_sic_set_motion_vector_cost_function(
        packed_cost_center_delta, packed_cost_table, cost_precision, payload);

    intel_sub_group_avc_sic_result_t result =
        intel_sub_group_avc_sic_evaluate_with_single_reference(
            srcImg, fwRefImg, accelerator, payload);

    ushort dists = intel_sub_group_avc_sic_get_inter_distortions(result);

    int index = (gid_0 * 16 + get_local_id(0)) +
                (gid_1 * 16 * get_num_groups(0)); // assumes 16 MV per MB

    if (skip_residuals != NULL)
      skip_residuals[index] = dists;
  }
}

/*************************************************************************************************\
Kernel: block_intrapred_intel

Description: Performs intraprediction on the given source frame. intraPartMask
indicates the block sizes (4x4,8x8,and/or
16x16) that are enabled.


Returns:  (i)  The best intra predictor mode in intra_search_predictor_modes.
This has 22 entries per 16x16 macroblock,
          and valid entries depend on the block sizes enabled and actually found
optimal.
          (ii) The actual shapes that each macroblock is partitioned into in
blk_size_buffer.
          (ii) The intraprediction residual error for each macroblock in
intra_residuals.

\*************************************************************************************************/

__kernel __attribute__((reqd_work_group_size(16, 1, 1))) void
block_intrapred_intel(
    __read_only image2d_t srcImg, uchar intraPartMask,
    __global char *intra_search_predictor_modes,
    __global ushort *intra_residuals,
    __global uchar *blk_size_buffer,
    __read_only image2d_t intra_src_img, // srcImg is used by VME media_block_read must use a
                                         // different image parameter; arguments passed into the
                                         // kernel for srcImg and intra_src_img will be the same
    int iterations) 
{
  sampler_t accelerator = 0;

  int gid_0 = get_group_id(0); // mb col
  int gid_1 = 0;               // mb row

  for (int i = 0; i < iterations; i++, gid_1++) {

    ushort2 srcCoord;

    srcCoord.x = gid_0 * 16 +
                 get_global_offset(0); // 16 pixels wide MBs (globally scalar)
    srcCoord.y = gid_1 * 16 +
                 get_global_offset(1); // 16 pixels tall MBs (globally scalar)

    uint curMB = gid_0 + gid_1 * get_num_groups(0); // current MB id

    uint intraEdges = 0;
    uchar leftEdge = 0;
    uchar leftUpperPixel = 0;
    uchar upperEdge = 0;
    uchar upperRightEdge = 0;

    intraEdges = CLK_AVC_ME_INTRA_NEIGHBOR_LEFT_MASK_ENABLE_INTEL |
                 CLK_AVC_ME_INTRA_NEIGHBOR_UPPER_MASK_ENABLE_INTEL |
                 CLK_AVC_ME_INTRA_NEIGHBOR_UPPER_LEFT_MASK_ENABLE_INTEL |
                 CLK_AVC_ME_INTRA_NEIGHBOR_UPPER_RIGHT_MASK_ENABLE_INTEL;

    // If this is a left-edge MB, then disable left edges.
    if ((gid_0 == 0) & (get_global_offset(0) == 0)) {
      intraEdges &= ~CLK_AVC_ME_INTRA_NEIGHBOR_LEFT_MASK_ENABLE_INTEL;
      intraEdges &= ~CLK_AVC_ME_INTRA_NEIGHBOR_UPPER_LEFT_MASK_ENABLE_INTEL;
    }
    // If this is a right edge MB then disable right edges.
    if (gid_0 == get_num_groups(0) - 1) {
      intraEdges &= ~CLK_AVC_ME_INTRA_NEIGHBOR_UPPER_RIGHT_MASK_ENABLE_INTEL;
    }
    // If this is a top-edge MB, then disable top edges.
    if ((gid_1 == 0) & (get_global_offset(1) == 0)) {
      intraEdges &= ~CLK_AVC_ME_INTRA_NEIGHBOR_UPPER_LEFT_MASK_ENABLE_INTEL;
      intraEdges &= ~CLK_AVC_ME_INTRA_NEIGHBOR_UPPER_RIGHT_MASK_ENABLE_INTEL;
      intraEdges &= ~CLK_AVC_ME_INTRA_NEIGHBOR_UPPER_MASK_ENABLE_INTEL;
    }

    // Read left edge
    {
      int2 edgeCoord;
      edgeCoord.x = srcCoord.x - 4;
      edgeCoord.y = srcCoord.y;

      uint leftEdgeDW =
          intel_sub_group_media_block_read_ui(edgeCoord, 1, 16, intra_src_img);
      leftEdge = as_uchar4(leftEdgeDW).s3;
    }

    // Read upper left corner

    {
      int2 edgeCoord;
      edgeCoord.x = srcCoord.x - 4;
      edgeCoord.y = srcCoord.y - 1;

      uint leftUpperPixelDW =
          intel_sub_group_media_block_read_ui(edgeCoord, 1, 16, intra_src_img);
      leftUpperPixel = as_uchar4(leftUpperPixelDW).s3;
      leftUpperPixel = intel_sub_group_shuffle(leftUpperPixel, 0);
    }

    // Read upper edge

    {
      int2 edgeCoord;
      edgeCoord.x = srcCoord.x;
      edgeCoord.y = srcCoord.y - 1;

      upperEdge =
          intel_sub_group_media_block_read_uc(edgeCoord, 16, 1, intra_src_img);
    }

    // Read upper right edge

    {
      int2 edgeCoord;
      edgeCoord.x = srcCoord.x + 16;
      edgeCoord.y = srcCoord.y - 1;

      upperRightEdge =
          intel_sub_group_media_block_read_uc(edgeCoord, 16, 1, intra_src_img);
    }

    uchar sad_adjustment = CLK_AVC_ME_SAD_ADJUST_MODE_NONE_INTEL;

    intel_sub_group_avc_sic_payload_t payload =
        intel_sub_group_avc_sic_initialize(srcCoord);

    payload = intel_sub_group_avc_sic_configure_ipe(
        intraPartMask, intraEdges, leftEdge, leftUpperPixel, upperEdge,
        upperRightEdge, sad_adjustment, payload);

    intel_sub_group_avc_sic_result_t result =
        intel_sub_group_avc_sic_evaluate_ipe(srcImg, accelerator, payload);

    uchar actual_blk_size = intel_sub_group_avc_sic_get_ipe_luma_shape(
        result); // actual block size that this MB is divided into
    ushort dist = intel_sub_group_avc_sic_get_best_ipe_luma_distortion(result);
    ulong modes = intel_sub_group_avc_sic_get_packed_ipe_luma_modes(result);

    int index;

    if (actual_blk_size == CLK_AVC_ME_INTRA_16x16_INTEL) {
      if (get_local_id(0) == 0) {
        char value = modes & 0xF;
        index = (gid_0 * 22) +
                (get_local_id(0)) + // 22 modes per MB, offset in MB = work-item
                (gid_1 * 22 * get_num_groups(0));

        intra_search_predictor_modes[index] = value; // mode 0 in MB
        if (value > 3)
          printf("\ninvalid intra mode returned gid_0 %d gid_1 %d blk_size 16, "
                 "work-item %d value %d\n",
                 gid_0, gid_1, get_local_id(0), value);
      }
    }

    else if (actual_blk_size == CLK_AVC_ME_INTRA_8x8_INTEL) {
      if (get_local_id(0) < 4) {
        uint shiftCount = get_local_id(0) * 16;
        char value = (modes >> shiftCount) & 0xF;

        index =
            (gid_0 * 22) + (get_local_id(0)) + (gid_1 * 22 * get_num_groups(0));

        intra_search_predictor_modes[index + 1] = value; // modes 1,2,3,4
        if (value > 8)
          printf("\ninvalid intra mode returned gid_0 %d gid_1 %d blk_size 8, "
                 "work-item %d value %d\n",
                 gid_0, gid_1, get_local_id(0), value);
      }
    }

    else if (actual_blk_size == CLK_AVC_ME_INTRA_4x4_INTEL) {
      // for 4x4 blk
      if (get_local_id(0) < 16) {

        uint shiftCount = get_local_id(0) * 4;
        char value = (modes >> shiftCount) & 0xF;

        index =
            (gid_0 * 22) + (get_local_id(0)) + (gid_1 * 22 * get_num_groups(0));

        intra_search_predictor_modes[index + 5] =
            value; // modes 5-20  (21 is for chroma, not currently used)
        if (value > 8)
          printf("\nInvalid intra mode returned gid_0 %d gid_1 %d blk_size 4, "
                 "work-item %d value %d\n",
                 gid_0, gid_1, get_local_id(0), value);
      }
    }

    if (get_local_id(0) == 0) {
      blk_size_buffer[curMB] = actual_blk_size;
      intra_residuals[curMB] = dist;
    }
  }
}
