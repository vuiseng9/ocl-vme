__kernel __attribute__((reqd_work_group_size(16,1,1)))
void  block_motion_estimate_intel(
    __read_only image2d_t   srcImg,
    __read_only image2d_t   refImg,
                uchar       interlaced,
                uchar       polarity,
    __global short2*        prediction_motion_vector_buffer,
    __global short2*        motion_vector_buffer,
    __global ushort*        residuals_buffer,
    __global uchar2*        shapes_buffer,
    int                     iterations ) 
{
  int gid_0 = get_group_id(0);
  int gid_1 = 0;
  sampler_t vme_sampler = CLK_AVC_ME_INITIALIZE_INTEL;

  for( int i = 0; i <  iterations ; i++, gid_1++ ) {
      ushort2 srcCoord = 0;
      short2 refCoord = 0;
      short2 predMV = 0;

      srcCoord.x = gid_0 * 16;
      srcCoord.y = gid_1 * 16;

      if(  prediction_motion_vector_buffer  != NULL ) {
          predMV =  prediction_motion_vector_buffer [ gid_0 + gid_1 * get_num_groups(0) ];
          refCoord.x = predMV.x / 4;
          refCoord.y = predMV.y / 4;
          refCoord.y = refCoord.y & 0xFFFE;
      }

      uchar partition_mask = CLK_AVC_ME_PARTITION_MASK_ALL_INTEL;
      uchar sad_adjustment = CLK_AVC_ME_SAD_ADJUST_MODE_NONE_INTEL;
      uchar pixel_mode = CLK_AVC_ME_SUBPIXEL_MODE_QPEL_INTEL;

      intel_sub_group_avc_ime_payload_t payload = intel_sub_group_avc_ime_initialize( srcCoord, partition_mask, sad_adjustment);
      payload = intel_sub_group_avc_ime_set_single_reference(refCoord, CLK_AVC_ME_SEARCH_WINDOW_EXHAUSTIVE_INTEL, payload);
      ulong cost_center = 0;
      uint2 packed_cost_table = intel_sub_group_avc_mce_get_default_high_penalty_cost_table();
      uchar search_cost_precision = CLK_AVC_ME_COST_PRECISION_QPEL_INTEL;
      payload = intel_sub_group_avc_ime_set_motion_vector_cost_function( cost_center, packed_cost_table, search_cost_precision, payload );
      if (interlaced) {
        payload = intel_sub_group_avc_ime_set_source_interlaced_field_polarity(polarity, payload );
        payload = intel_sub_group_avc_ime_set_single_reference_interlaced_field_polarity(polarity, payload);
      }
      intel_sub_group_avc_ime_result_t result = intel_sub_group_avc_ime_evaluate_with_single_reference(srcImg, refImg, vme_sampler, payload );

      // Process Results
      long mvs           = intel_sub_group_avc_ime_get_motion_vectors( result );
      ushort dists       = intel_sub_group_avc_ime_get_inter_distortions( result );
      uchar major_shape  = intel_sub_group_avc_ime_get_inter_major_shape( result );
      uchar minor_shapes = intel_sub_group_avc_ime_get_inter_minor_shapes( result );
      uchar2 shapes      = { major_shape, minor_shapes };
      uchar directions   = intel_sub_group_avc_ime_get_inter_directions( result );

      // Perform FME for non-integer p.ixel mode
      if( pixel_mode != CLK_AVC_ME_SUBPIXEL_MODE_INTEGER_INTEL ) {
         intel_sub_group_avc_ref_payload_t payload = intel_sub_group_avc_fme_initialize( srcCoord, mvs, major_shape, minor_shapes, directions, pixel_mode, sad_adjustment);
         payload = intel_sub_group_avc_ref_set_motion_vector_cost_function(cost_center,packed_cost_table,search_cost_precision,payload );
         if (interlaced) {
            payload = intel_sub_group_avc_ref_set_source_interlaced_field_polarity(polarity, payload );
            payload = intel_sub_group_avc_ref_set_single_reference_interlaced_field_polarity(polarity, payload);
         }
         intel_sub_group_avc_ref_result_t result = intel_sub_group_avc_ref_evaluate_with_single_reference( srcImg, refImg, vme_sampler, payload );
         mvs = intel_sub_group_avc_ref_get_motion_vectors( result );
         dists = intel_sub_group_avc_ref_get_inter_distortions( result );
      }

      // Do a skip check to verify distortions (just for validation or demo purposes).

      if( ( major_shape == CLK_AVC_ME_MAJOR_16x16_INTEL ) ||
          ( major_shape == CLK_AVC_ME_MAJOR_8x8_INTEL && 
            minor_shapes == CLK_AVC_ME_MINOR_8x8_INTEL ) )
      {
          uint skip_block_partition_type = 
              ( major_shape == CLK_AVC_ME_MAJOR_16x16_INTEL )? 
               CLK_AVC_ME_SKIP_BLOCK_PARTITION_16x16_INTEL : CLK_AVC_ME_SKIP_BLOCK_PARTITION_8x8_INTEL;
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
	      
          if (interlaced) {
            check_payload = intel_sub_group_avc_sic_set_source_interlaced_field_polarity(polarity, check_payload );
            check_payload = intel_sub_group_avc_sic_set_single_reference_interlaced_field_polarity(polarity, check_payload);
          }

	      intel_sub_group_avc_sic_result_t checked_result =
		      intel_sub_group_avc_sic_evaluate_with_single_reference(
			      srcImg,
			      refImg,
			      vme_sampler,
			      check_payload );

	      ushort checked_dists = intel_sub_group_avc_sic_get_inter_distortions( checked_result );

	      if( checked_dists != dists )
	      {
		      if( get_sub_group_local_id() == 0 ||  get_sub_group_local_id() == 4 )
		      {
			      printf(" *ERROR 2* : %d %d (%X : %d,%d :%d)\n", checked_dists, dists, skip_motion_vector_mask >> 24, gid_0, gid_1, get_sub_group_local_id());
		      }
	      }
      }
      
      // Write out results
      int index = ( gid_0 * 16 + get_local_id(0) ) + ( gid_1 * 16 * get_num_groups(0) );
      int2 bi_mvs = as_int2( mvs );
      motion_vector_buffer [ index ] = as_short2( bi_mvs.s0 );
      if(  residuals_buffer  != NULL ) {
        residuals_buffer [ index ] = dists;
      }
      shapes_buffer [gid_0 + gid_1 * get_num_groups(0)] = shapes;
  }
}
