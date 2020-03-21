/*************************************************************************************************\
 Built-in kernel:
    block_advanced_motion_estimate_check_intel

 Description:

    1. Do motion estimation with 0 to 8 predictor MVs using 0 to 8 (integer motion estimation) IMEs per macro-block, calculating the best search MVs per specified (16x16, 8x8, 4x4) luma block with lowest distortion from amongst the 0 to 8 IME results, and optionally do (fractional bi-directional refinement) FBR on the best IME search results to refine the best search results. The best search (FBR if done, or IME) MVs and their distortions are returned.
    2. Do skip (zero search) checks with 0 to 8 sets of skip check MVs for (16x16, 8x8) luma blocks using 0 to 8 (skip and intra check) SICs and return the distortions associated with the input sets of skip check MVs per specified luma block. 4x4 blocks are not supported by h/w for skip checks.
    3. Do intra-prediction for (16x16, 8x8, 4x4) luma blocks and (8x8) chroma blocks using 3 SICs and returning the predictor modes and their associated distortions. Intra-prediction is done for all block sizes. Support for 8x8 chroma blocks cannot be enabled until NV image formats are supported in OCL.

\*************************************************************************************************/

inline intel_sub_group_avc_sic_result_t
sic_eval_intra(
    sampler_t               accelerator,
    __read_only image2d_t   srcImg,                              
    __read_only image2d_t   refImg,
	bool                    doChromaIntra,
    int                     intraPartMask,
    uint                    neighborMask,
    uchar                   leftLumaEdge,
    uchar                   leftUpperLumaPixel,
    uchar                   upperLumaEdge,
    uchar                   upperRightLumaEdge,
    ushort                  leftChromaEdge,
    ushort                  leftUpperChromaPixel,
    ushort                  upperChromaEdge,
    ushort2                 srcCoord,
    uchar                   sad_adjustment,
    uchar                   pixel_mode )
{
  intel_sub_group_avc_sic_payload_t payload = intel_sub_group_avc_sic_initialize( srcCoord );
  if( doChromaIntra ) {
	  payload =
		intel_sub_group_avc_sic_configure_ipe(
									intraPartMask,
									neighborMask,
									leftLumaEdge,
									leftUpperLumaPixel,
									upperLumaEdge,
									upperRightLumaEdge,
									leftChromaEdge,
									leftUpperChromaPixel,
									upperChromaEdge,
									sad_adjustment,
									payload );
  }
  else {
	  payload =
		intel_sub_group_avc_sic_configure_ipe(
									intraPartMask,
									neighborMask,
									leftLumaEdge,
									leftUpperLumaPixel,
									upperLumaEdge,
									upperRightLumaEdge,
									sad_adjustment,
									payload );
  }
  intel_sub_group_avc_sic_result_t result;
  result =
    intel_sub_group_avc_sic_evaluate_ipe(
                               srcImg,
                               accelerator,
                               payload );
  return result;
}

inline intel_sub_group_avc_sic_result_t
sic_eval_skc_intra(
    sampler_t               accelerator,
    __read_only image2d_t   srcImg,                              
    __read_only image2d_t   refImg,
    bool                    doIntra,
    bool                    doChromaIntra,
    bool                    is8x8,
    int                     intraPartMask,
    uint                    neighborMask,
    uchar                   leftLumaEdge,
    uchar                   leftUpperLumaPixel,
    uchar                   upperLumaEdge,
    uchar                   upperRightLumaEdge,
    ushort                  leftChromaEdge,
    ushort                  leftUpperChromaPixel,
    ushort                  upperChromaEdge,
    ushort2                 srcCoord,
    uchar                   sad_adjustment,
	short2                  packedRefCoords,
    uint                    skipMV,
    int                     component,
    uchar                   pixel_mode,
	uchar                   cost_precision,
    uchar                   cost_penalty )
{
  uint skip_block_partition_type = 0;
  uint skip_motion_vector_mask = 0;
  uint packedMVs = 0;

  // Extract the ref coords for the n'th SIC.

  int refCoords_int = as_int( packedRefCoords );
  int refCoord_int = intel_sub_group_shuffle( refCoords_int, component );
  short2 refCoord = as_short2( refCoord_int );

  // Extract the MVs for the n'th SIC.

  if( is8x8 ) {
    uint ids = 4 * ( component % 4 ) + ( get_sub_group_local_id() % 4 );
    packedMVs = intel_sub_group_shuffle( skipMV, ids );
    skip_block_partition_type = CLK_AVC_ME_SKIP_BLOCK_PARTITION_8x8_INTEL;
    skip_motion_vector_mask = CLK_AVC_ME_SKIP_BLOCK_8x8_FORWARD_ENABLE_INTEL;
  }
  else {
    uint ids = component;
    packedMVs = intel_sub_group_shuffle( skipMV, ids );
    skip_block_partition_type = CLK_AVC_ME_SKIP_BLOCK_PARTITION_16x16_INTEL;
    skip_motion_vector_mask = CLK_AVC_ME_SKIP_BLOCK_16x16_FORWARD_ENABLE_INTEL;
  }

  // Convert to BMV format.
  uint2 mv_int;
  mv_int.s0 = packedMVs;
  mv_int.s1 = 0;
  long mv = as_long( mv_int );

  intel_sub_group_avc_sic_payload_t payload = intel_sub_group_avc_sic_initialize( srcCoord );
  uchar bidirectional_weight = 0;
  payload =
    intel_sub_group_avc_sic_configure_skc(
                                skip_block_partition_type,
                                skip_motion_vector_mask,
                                mv,
                                bidirectional_weight,
                                sad_adjustment,
                                payload );

  if( doIntra ) {
    if( doChromaIntra ) {
		payload =
		  intel_sub_group_avc_sic_configure_ipe(
									  intraPartMask,
									  neighborMask,
									  leftLumaEdge,
									  leftUpperLumaPixel,
									  upperLumaEdge,
									  upperRightLumaEdge,
									  leftChromaEdge,
									  leftUpperChromaPixel,
									  upperChromaEdge,
									  sad_adjustment,
									  payload );
	}
	else {
		payload =
		  intel_sub_group_avc_sic_configure_ipe(
									  intraPartMask,
									  neighborMask,
									  leftLumaEdge,
									  leftUpperLumaPixel,
									  upperLumaEdge,
									  upperRightLumaEdge,
									  sad_adjustment,
									  payload );
	}
  }

  uint2 packed_cost_table = 0;

  if (cost_penalty == 1)
    packed_cost_table = intel_sub_group_avc_mce_get_default_low_penalty_cost_table();
  else if (cost_penalty == 2)
    packed_cost_table = intel_sub_group_avc_mce_get_default_medium_penalty_cost_table();
  else if (cost_penalty == 3)
    packed_cost_table = intel_sub_group_avc_mce_get_default_high_penalty_cost_table();

  // Cost coords are in QPEL resolution.

  int costCoord_int = intel_sub_group_shuffle( refCoords_int, 0 );
  short2 costCoord = as_short2( costCoord_int );
  costCoord.s0 = costCoord.s0 << 2;
  costCoord.s1 = costCoord.s1 << 2;
  uint2 packed_cost_center_delta_int;
  packed_cost_center_delta_int.s0 = as_uint( costCoord );
  packed_cost_center_delta_int.s1 = 0;
  ulong packed_cost_center_delta = as_ulong( packed_cost_center_delta_int );

  payload =
    intel_sub_group_avc_sic_set_motion_vector_cost_function(
                                    packed_cost_center_delta,        
                                    packed_cost_table,
                                    cost_precision,
                                    payload );

  intel_sub_group_avc_sic_result_t result =
    intel_sub_group_avc_sic_evaluate_with_single_reference(
                                                 srcImg,
                                                 refImg,
                                                 accelerator,
                                                 payload );

  return result;
}

inline intel_sub_group_avc_sic_result_t
sic_eval(
    sampler_t               accelerator,
    __read_only image2d_t   srcImg,                              
    __read_only image2d_t   refImg,
    bool                    doSKC,
    bool                    doIntra,
    bool                    doChromaIntra,
    bool                    is8x8,
    int                     intraPartMask,
    uint                    neighborMask,
    uchar                   leftLumaEdge,
    uchar                   leftUpperLumaPixel,
    uchar                   upperLumaEdge,
    uchar                   upperRightLumaEdge,
    ushort                  leftChromaEdge,
    ushort                  leftUpperChromaPixel,
    ushort                  upperChromaEdge,
    ushort2                 srcCoord,
    uchar                   sad_adjustment,
	short2                  packedRefCoords,
    uint                    skipMV,
    int                     component,
    uchar                   pixel_mode,
	uchar                   cost_precision,
    uchar                   cost_penalty )
{
  intel_sub_group_avc_sic_result_t result;

  if( doSKC ) {
    result =
      sic_eval_skc_intra(
                         accelerator,
                         srcImg,                              
                         refImg,
                         doIntra,
						 doChromaIntra,
                         is8x8,
                         intraPartMask,
                         neighborMask,
                         leftLumaEdge,
                         leftUpperLumaPixel,
                         upperLumaEdge,
                         upperRightLumaEdge,
                         leftChromaEdge,
                         leftUpperChromaPixel,
                         upperChromaEdge,
                         srcCoord,
                         sad_adjustment,
						 packedRefCoords,
                         skipMV,
                         component,
                         pixel_mode,
						 cost_precision,
						 cost_penalty );
  }
  else {
    result =
      sic_eval_intra(
                     accelerator,
                     srcImg,                              
                     refImg,
					 doChromaIntra,
                     intraPartMask,
                     neighborMask,
                     leftLumaEdge,
                     leftUpperLumaPixel,
                     upperLumaEdge,
                     upperRightLumaEdge,
                     leftChromaEdge,
                     leftUpperChromaPixel,
                     upperChromaEdge,
                     srcCoord,
                     sad_adjustment,
                     pixel_mode );
  }

  return result;
}

inline intel_sub_group_avc_ime_result_t
ime_eval(
    sampler_t               accelerator,
    __read_only image2d_t   srcImg,                              
    __read_only image2d_t   refImg,
    ushort2                 srcCoord,
    uchar                   partition_mask,
    uchar                   sad_adjustment,
    short2                  packedRefCoords,
    int                     component,
    uchar                   pixel_mode,
    uchar                   cost_precision,
    uchar                   cost_penalty )
{
  intel_sub_group_avc_ime_payload_t payload =
    intel_sub_group_avc_ime_initialize(
                             srcCoord,
                             partition_mask,
                             sad_adjustment);

  // Extract the ref coords for the n'th IME.

  int refCoords_int = as_int( packedRefCoords );
  int refCoord_int = intel_sub_group_shuffle( refCoords_int, component );
  short2 refCoord = as_short2( refCoord_int );

  payload =
    intel_sub_group_avc_ime_set_single_reference(
                                       refCoord,
                                       CLK_AVC_ME_SEARCH_WINDOW_16x12_RADIUS_INTEL,
                                       payload );
  uint2 packed_cost_table = 0;

  if (cost_penalty == 1)
    packed_cost_table = intel_sub_group_avc_mce_get_default_low_penalty_cost_table();
  else if (cost_penalty == 2)
    packed_cost_table = intel_sub_group_avc_mce_get_default_medium_penalty_cost_table();
  else if (cost_penalty == 3)
    packed_cost_table = intel_sub_group_avc_mce_get_default_high_penalty_cost_table();

  // Cost coords are in QPEL resolution.

  int costCoord_int = intel_sub_group_shuffle( refCoords_int, 0 );
  short2 costCoord = as_short2( costCoord_int );
  costCoord.s0 = costCoord.s0 << 2;
  costCoord.s1 = costCoord.s1 << 2;
  uint2 packed_cost_center_delta_int;
  packed_cost_center_delta_int.s0 = as_uint( costCoord );
  packed_cost_center_delta_int.s1 = 0;
  ulong packed_cost_center_delta = as_ulong( packed_cost_center_delta_int );

  payload =
    intel_sub_group_avc_ime_set_motion_vector_cost_function(
                                    packed_cost_center_delta,        
                                    packed_cost_table,
                                    cost_precision,
                                    payload );

  intel_sub_group_avc_ime_result_t result;
  result =
    intel_sub_group_avc_ime_evaluate_with_single_reference(
                                   srcImg,
                                   refImg,
                                   accelerator,
                                   payload );

  return result;
}

inline ushort
extract_distortion_h1(
    ushort udists32,
    bool is8x8,
    int component,
    ushort initial_packed_dists )
{
  ushort packed_dists = initial_packed_dists;

  uint ids =
    is8x8 ?
    ( get_sub_group_local_id() - component * 4 ) * 4:
    get_sub_group_local_id() - component;
  ids = ids < 16 ? ids : 0;
  ushort ldists = intel_sub_group_shuffle( udists32, ids );

  if( is8x8 ) {
    if( ( get_sub_group_local_id() >= component * 4 ) &
        ( get_sub_group_local_id() < component * 4 + 4 ) ) {
      packed_dists = ldists;
    }
  }
  else {
    if( get_sub_group_local_id() == component ) {
      packed_dists = ldists;
    }
  }

  return packed_dists;
}

inline ushort
extract_distortion_h2(
    ushort udists32,
    bool is8x8,
    int component,
    ushort initial_packed_dists )
{
  ushort packed_dists = initial_packed_dists;

  if( is8x8 ) {
    component -= 4;
    uint ids = ( get_sub_group_local_id() - component * 4 ) * 4;
    ids = ids < 16 ? ids : 0;
    ushort ldists = intel_sub_group_shuffle( udists32, ids ); 
    if( ( get_sub_group_local_id() >= component * 4 ) &
        ( get_sub_group_local_id() < component * 4 + 4 ) ) {
      packed_dists = ldists;
    }  
  }
  else {
    uint ids = get_sub_group_local_id() - component;
    ids = ids < 16 ? ids : 0;
    ushort ldists = intel_sub_group_shuffle( udists32, ids );  
    if( get_sub_group_local_id() == component ) {
      packed_dists = ldists;
    }
  }

  return packed_dists;
}

__kernel __attribute__((reqd_work_group_size(16,1,1)))
void  block_advanced_motion_estimate_check_intel(
    __read_only image2d_t       srcImg,
    __read_only image2d_t       refImg,
    uint                        flags,
    uint                        skip_block_type,
    uint                        search_cost_penalty,
    uint                        search_cost_precision,
    __global short2            *count_motion_vector_buffer,
    __global short2            *prediction_motion_vector_buffer,
    __global short2            *skip_motion_vector_buffer,
    __global short2            *search_motion_vector_buffer,
    __global char              *intra_search_predictor_modes,
    __global ushort            *search_residuals,
    __global ushort            *skip_residuals,
    __global ushort            *intra_residuals,
    __read_only image2d_t       intra_src_luma_image,
    __read_only image2d_t       intra_src_chroma_image,
    int                         iterations,
    uchar             partition_mask,
    uchar             sad_adjustment,
    uchar             pixel_mode )
{
  // Frame is divided into rows * columns of MBs.
  // One h/w thread per WG.
  // One WG processes "row" MBs - one row per iteration and one MB per row.
  // Number of WGs (or h/w threads) is number of columns MBs
  // Each iteration processes the MB in a row - gid_0 is the MB id in a row and gid_1 is the row offset.

  int gid_0 = get_group_id(0);
  int gid_1 = 0;
  sampler_t accelerator = CLK_AVC_ME_INITIALIZE_INTEL;
  for( int i = 0; i < iterations; i++, gid_1++ ) {
    ushort2 srcCoord;
    short2 refCoords = 0;

    srcCoord.x = gid_0 * 16 + get_global_offset(0);       // 16 pixels wide MBs (globally scalar)
    srcCoord.y = gid_1 * 16 + get_global_offset(1);       // 16 pixels tall MBs (globally scalar)

    uint curMB = gid_0 + gid_1 * get_num_groups(0);       // current MB id

    short2 count = count_motion_vector_buffer [ curMB ];

    //-------------------------------------------------------------------------

    int countPredMVs = count.x;
    if( countPredMVs != 0 ) {
      uint offset = curMB * 8;                 // 8 predictors per MB
      offset += get_local_id(0) % 8;           // 16 work-items access 8 MVs for MB
                                               // one predictor for MB per SIMD channel

      // Reduce predictors from QPEL to PEL resolution.

      int2 predMV = 0;

      if( get_local_id(0) < countPredMVs ) {
        predMV = convert_int2( prediction_motion_vector_buffer [ offset ] );
        refCoords.x = predMV.x / 4;                                             
        refCoords.y = predMV.y / 4;                                                       
        refCoords.y = refCoords.y & 0xFFFE;
      }
    
      long mvs_best;
      ushort dists_best;

      intel_sub_group_avc_ime_result_t result0;
      result0 =
        ime_eval(
                 accelerator,
                 srcImg,
                 refImg,
                 srcCoord,
                 partition_mask,
                 sad_adjustment,
                 refCoords,
                 0,
                 pixel_mode,
                 search_cost_precision,
                 search_cost_penalty );

      uchar major_shape  = intel_sub_group_avc_ime_get_inter_major_shape( result0 );
      uchar minor_shapes = intel_sub_group_avc_ime_get_inter_minor_shapes( result0 );
      uchar directions   = intel_sub_group_avc_ime_get_inter_directions( result0 );

      // NOTE: The initialzers are to limit register pressure.

      intel_sub_group_avc_ime_result_t result1 = result0;
      intel_sub_group_avc_ime_result_t result2 = result0;
      intel_sub_group_avc_ime_result_t result3 = result0;

      if( countPredMVs > 1 ) {
        result1 =
          ime_eval(
                   accelerator,
                   srcImg,                              
                   refImg,
                   srcCoord,
                   partition_mask,
                   sad_adjustment,
                   refCoords,
                   1,
                   pixel_mode,
                   search_cost_precision,
                   search_cost_penalty  );
      }

      if( countPredMVs > 2 ) {
        result2 =
          ime_eval(
                   accelerator,
                   srcImg,                              
                   refImg,
                   srcCoord,
                   partition_mask,
                   sad_adjustment,
                   refCoords,
                   2,
                   pixel_mode,
                   search_cost_precision,
                   search_cost_penalty  );
      }

      if( countPredMVs > 3 ) {
        result3 =
          ime_eval(
                   accelerator,
                   srcImg,                              
                   refImg,
                   srcCoord,
                   partition_mask,
                   sad_adjustment,
                   refCoords,
                   3,
                   pixel_mode,
                   search_cost_precision,
                   search_cost_penalty  );
      }

      mvs_best = intel_sub_group_avc_ime_get_motion_vectors( result0 );
      dists_best = intel_sub_group_avc_ime_get_inter_distortions( result0 );

      if( countPredMVs > 1 ) {
        long mvs1 = intel_sub_group_avc_ime_get_motion_vectors( result1 );
        ushort dists1 = intel_sub_group_avc_ime_get_inter_distortions( result1 );
        if( dists1 < dists_best ) {
          mvs_best = mvs1;
          dists_best = dists1;
        }
      }

      if( countPredMVs > 2 ) {
        long mvs2 = intel_sub_group_avc_ime_get_motion_vectors( result2 );
        ushort dists2 = intel_sub_group_avc_ime_get_inter_distortions( result2 );
        if( dists2 < dists_best ) {
          mvs_best = mvs2;
          dists_best = dists2;
        }
      }

      if( countPredMVs > 3 ) {
        long mvs3 = intel_sub_group_avc_ime_get_motion_vectors( result3 );
        ushort dists3 = intel_sub_group_avc_ime_get_inter_distortions( result3 );
        if( dists3 < dists_best ) {
          mvs_best = mvs3;
          dists_best = dists3;
        }
      }

      if( countPredMVs > 4 ) {
        intel_sub_group_avc_ime_result_t result4;
        result4 =
          ime_eval(
                   accelerator,
                   srcImg,
                   refImg,
                   srcCoord,
                   partition_mask,
                   sad_adjustment,
                   refCoords,
                   4,
                   pixel_mode,
                   search_cost_precision,
                   search_cost_penalty );

        // NOTE: The initialzers are to limit register pressure.

        intel_sub_group_avc_ime_result_t result5 = result4;
        intel_sub_group_avc_ime_result_t result6 = result4;
        intel_sub_group_avc_ime_result_t result7 = result4;

        if( countPredMVs > 5 ) {
          result5 =
            ime_eval(
                     accelerator,
                     srcImg,                              
                     refImg,
                     srcCoord,
                     partition_mask,
                     sad_adjustment,
                     refCoords,
                     5,
                     pixel_mode,
                     search_cost_precision,
                     search_cost_penalty  );
        }

        if( countPredMVs > 6 ) {
          result6 =
            ime_eval(
                     accelerator,
                     srcImg,                              
                     refImg,
                     srcCoord,
                     partition_mask,
                     sad_adjustment,
                     refCoords,
                     6,
                     pixel_mode,
                     search_cost_precision,
                     search_cost_penalty  );
        }

        if( countPredMVs > 7 ) {
          result7 =
            ime_eval(
                     accelerator,
                     srcImg,                              
                     refImg,
                     srcCoord,
                     partition_mask,
                     sad_adjustment,
                     refCoords,
                     7,
                     pixel_mode,
                     search_cost_precision,
                     search_cost_penalty  );
        }

        long mvs4 = intel_sub_group_avc_ime_get_motion_vectors( result4 );
        ushort dists4 = intel_sub_group_avc_ime_get_inter_distortions( result4 );
        if( dists4 < dists_best ) {
          mvs_best = mvs4;
          dists_best = dists4;
        }

        if( countPredMVs > 5 ) {
          long mvs5 = intel_sub_group_avc_ime_get_motion_vectors( result5 );
          ushort dists5 = intel_sub_group_avc_ime_get_inter_distortions( result5 );
          if( dists5 < dists_best ) {
            mvs_best = mvs5;
            dists_best = dists5;
          }
        }

        if( countPredMVs > 6 ) {
          long mvs6 = intel_sub_group_avc_ime_get_motion_vectors( result6 );
          ushort dists6 = intel_sub_group_avc_ime_get_inter_distortions( result6 );
          if( dists6 < dists_best ) {
            mvs_best = mvs6;
            dists_best = dists6;
          }
        }

        if( countPredMVs > 7 ) {
          long mvs7 = intel_sub_group_avc_ime_get_motion_vectors( result7 );
          ushort dists7 = intel_sub_group_avc_ime_get_inter_distortions( result7 );
          if( dists7 < dists_best ) {
            mvs_best = mvs7;
            dists_best = dists7;
          }
        }
      }

      long mvs;
      ushort dists;

      if( pixel_mode != CLK_AVC_ME_SUBPIXEL_MODE_INTEGER_INTEL ) {

        // Replicate relevant MV components for 16x16 and 8x8 to unused MV components
        // prior to FME. This is needed to properly reconstruct the input MVs for FME.

    if (partition_mask == CLK_AVC_ME_PARTITION_MASK_16x16_INTEL) {
          mvs_best = intel_sub_group_shuffle( mvs_best, 0 );
        }
    else if (partition_mask == CLK_AVC_ME_PARTITION_MASK_8x8_INTEL) {
          long v0 = intel_sub_group_shuffle( mvs_best, 0 );
          long v1 = intel_sub_group_shuffle( mvs_best, 4 );
          long v2 = intel_sub_group_shuffle( mvs_best, 8 );
          long v3 = intel_sub_group_shuffle( mvs_best, 12 );
          if( get_local_id(0) < 4 )
            mvs_best = v0;
          else if( get_local_id(0) < 8 )
            mvs_best = v1;
          else if( get_local_id(0) < 12 )
            mvs_best = v2;
          else                    
            mvs_best = v3;
        }

        intel_sub_group_avc_ref_payload_t payload =
          intel_sub_group_avc_fme_initialize(
                                   srcCoord,
                                   mvs_best,
                                   major_shape,
                                   minor_shapes,
                                   directions,
                                   pixel_mode,
                                   sad_adjustment);

        uint2 packed_cost_table = 0;
        if (search_cost_penalty == 1)
          packed_cost_table = intel_sub_group_avc_mce_get_default_low_penalty_cost_table();
        else if (search_cost_penalty == 2)
          packed_cost_table = intel_sub_group_avc_mce_get_default_medium_penalty_cost_table();
        else if (search_cost_penalty == 3)
          packed_cost_table = intel_sub_group_avc_mce_get_default_high_penalty_cost_table();

        int refCoords_int = as_int( refCoords );
        int refCoord_int = intel_sub_group_shuffle( refCoords_int, 0 );
        short2 costCoord = as_short2( refCoord_int );
        costCoord.s0 = costCoord.s0 << 2;
        costCoord.s1 = costCoord.s1 << 2;
        uint2 packed_cost_center_delta_int;
        packed_cost_center_delta_int.s0 = as_uint( costCoord );
        packed_cost_center_delta_int.s1 = 0;
        ulong packed_cost_center_delta = as_ulong( packed_cost_center_delta_int );

        payload =
          intel_sub_group_avc_ref_set_motion_vector_cost_function(
                                          packed_cost_center_delta,  
                                          packed_cost_table,
                                          search_cost_precision,
                                          payload );

        intel_sub_group_avc_ref_result_t result =
          intel_sub_group_avc_ref_evaluate_with_single_reference(
                                                       srcImg,
                                                       refImg,
                                                       accelerator,
                                                       payload );

        mvs = intel_sub_group_avc_ref_get_motion_vectors( result );
        dists = intel_sub_group_avc_ref_get_inter_distortions( result );
      }
      else {
        mvs = mvs_best;
        dists = dists_best;
      }

      int2 bi_mvs = as_int2( mvs );
      int fwd_mvs = bi_mvs.s0;

      // Write Out Result                                                                  
                                                                                             
      // 4x4                                                                               
    if (partition_mask == CLK_AVC_ME_PARTITION_MASK_4x4_INTEL) {                                                                                    
          int index =
            ( gid_0 * 16 + get_local_id(0) ) +
            ( gid_1 * 16 * get_num_groups(0) );

          int2 bi_mvs = as_int2( mvs );
          int fwd_mvs = bi_mvs.s0;
          short2 val = as_short2( fwd_mvs );
          search_motion_vector_buffer [ index ] = val;

          if(  search_residuals  != NULL ) {
            search_residuals [ index ] = dists;
          }                                 
        }                                                                                    
                                                                                             
      // 8x8                                                                               
    if (partition_mask == CLK_AVC_ME_PARTITION_MASK_8x8_INTEL) {
        uint dists32 = dists;
        uint ids = get_sub_group_local_id() < 4 ? get_sub_group_local_id() * 4: 0;                
        uint dist_8x8 = intel_sub_group_shuffle( dists32, ids );

        long bi_mvs = intel_sub_group_shuffle( mvs, ids );
        int2 bi_mvs_8x8 = as_int2( bi_mvs );
        int fwd_mvs_8x8 = bi_mvs_8x8.s0;
                                                                                                   

        if( get_local_id(0) < 4 ) {
          int index =
            ( gid_0 * 4 + get_local_id(0) ) +
            ( gid_1 * 4 * get_num_groups(0) );                             
      
          short2 val = as_short2( fwd_mvs_8x8 );
          search_motion_vector_buffer [ index ] = val;

          if( search_residuals  != NULL ) {
            search_residuals [ index ] = dist_8x8;
          }                                                                            
        }                                                                                
      }                                                                                    

      // 16x16                                                                             
    if (partition_mask == CLK_AVC_ME_PARTITION_MASK_16x16_INTEL) {
        if( get_local_id(0) == 0 )  {
          int index =
            gid_0 +
            gid_1 * get_num_groups(0);                                            
          
          int2 bi_mvs = as_int2( mvs );
          int fwd_mvs = bi_mvs.s0;
  
          short2 val = as_short2( fwd_mvs );
          search_motion_vector_buffer [ index ] = val;                            

          if(  search_residuals  != NULL ) {
            search_residuals [ index ] = dists;
          }                                                                            
        }                                                                               
      }
    }

    //-------------------------------------------------------------------------


    int doIntra = ( ( flags & 0x2 ) != 0 );
	int doChromaIntra = ( ( flags & 0x1 ) != 0 );
    uint intraEdges = 0;
    uchar leftLumaEdge = 0;
    uchar leftUpperLumaPixel = 0;
    uchar upperLumaEdge = 0;
    uchar upperRightLumaEdge = 0;
    ushort leftChromaEdge = 0;
    ushort leftUpperChromaPixel = 0;
    ushort upperChromaEdge = 0;

    if( doIntra ) {
      intraEdges =
        CLK_AVC_ME_INTRA_NEIGHBOR_LEFT_MASK_ENABLE_INTEL       |
        CLK_AVC_ME_INTRA_NEIGHBOR_UPPER_MASK_ENABLE_INTEL      |
        CLK_AVC_ME_INTRA_NEIGHBOR_UPPER_LEFT_MASK_ENABLE_INTEL |
        CLK_AVC_ME_INTRA_NEIGHBOR_UPPER_RIGHT_MASK_ENABLE_INTEL;

      // If this is a left-edge MB, then disable left edges.
      if( ( gid_0 == 0 ) &
          ( get_global_offset(0) == 0 ) ) {
        intraEdges &= ~CLK_AVC_ME_INTRA_NEIGHBOR_LEFT_MASK_ENABLE_INTEL;
        intraEdges &= ~CLK_AVC_ME_INTRA_NEIGHBOR_UPPER_LEFT_MASK_ENABLE_INTEL;
      }
      // If this is a right edge MB then disable right edges.
      if( gid_0 == get_num_groups(0) - 1 ) {
        intraEdges &= ~CLK_AVC_ME_INTRA_NEIGHBOR_UPPER_RIGHT_MASK_ENABLE_INTEL;
      }
      // If this is a top-edge MB, then disable top edges.
      if( ( gid_1 == 0 ) &
          ( get_global_offset(1) == 0 ) ) {
        intraEdges &= ~CLK_AVC_ME_INTRA_NEIGHBOR_UPPER_LEFT_MASK_ENABLE_INTEL;
        intraEdges &= ~CLK_AVC_ME_INTRA_NEIGHBOR_UPPER_RIGHT_MASK_ENABLE_INTEL;
        intraEdges &= ~CLK_AVC_ME_INTRA_NEIGHBOR_UPPER_MASK_ENABLE_INTEL;
      }

      // Read left luma edge from luma plane.
      {
        int2 edgeCoord;
        edgeCoord.x = srcCoord.x - 4;
        edgeCoord.y = srcCoord.y;
        uint leftLumaEdgeDW =
          intel_sub_group_media_block_read_ui(
                                    edgeCoord,
                                    1,
                                    16,
                                    intra_src_luma_image );
        leftLumaEdge = as_uchar4( leftLumaEdgeDW ).s3;
      }

      // Read upper left luma corner from luma plane.
      {
        int2 edgeCoord;
        edgeCoord.x = srcCoord.x - 4;
        edgeCoord.y = srcCoord.y - 1;
        uint leftUpperLumaPixelDW =
          intel_sub_group_media_block_read_ui(
                                    edgeCoord,
                                    1,
                                    16,
                                    intra_src_luma_image );
        leftUpperLumaPixel = as_uchar4( leftUpperLumaPixelDW ).s3;
        leftUpperLumaPixel = intel_sub_group_shuffle( leftUpperLumaPixel, 0 );
      }

    // Read upper luma edge from luma plane.
      {
        int2 edgeCoord;
        edgeCoord.x = srcCoord.x;
        edgeCoord.y = srcCoord.y - 1;
        upperLumaEdge =
          intel_sub_group_media_block_read_uc(
                                    edgeCoord,
                                    16,
                                    1,
                                    intra_src_luma_image );
      }

      // Read upper right luma edge from luma plane.
      {
        int2 edgeCoord;
        edgeCoord.x = srcCoord.x + 16;
        edgeCoord.y = srcCoord.y - 1;
        upperRightLumaEdge =
          intel_sub_group_media_block_read_uc(
                                    edgeCoord,
                                    16,
                                    1,
                                    intra_src_luma_image );
      }

      // Read left chroma edge from chroma plane.	 
	  if( doChromaIntra ) 
      {
        int2 edgeCoord;
        edgeCoord.x = srcCoord.x - 4;
        edgeCoord.y = srcCoord.y / 2;
        uint leftChromaEdgeDW =
          intel_sub_group_media_block_read_ui(
                                    edgeCoord,
                                    1,
                                    8,
                                    intra_src_chroma_image );
        leftChromaEdge = as_ushort2( leftChromaEdgeDW ).s1;
      }

      // Read upper left chroma corner from chroma plane.
	  if( doChromaIntra )
      {
        int2 edgeCoord;
        edgeCoord.x = srcCoord.x - 4;
        edgeCoord.y = srcCoord.y / 2 - 1;
        uint leftUpperChromaPixelDW =
          intel_sub_group_media_block_read_ui(
                                    edgeCoord,
                                    1,
                                    8,
                                    intra_src_chroma_image );
        leftUpperChromaPixel = as_ushort2( leftUpperChromaPixelDW ).s1;
        leftUpperChromaPixel = intel_sub_group_shuffle( leftUpperChromaPixel, 0 );
      }

      // Read upper chroma edge from chroma plane.
	  if( doChromaIntra )
      {
        int2 edgeCoord;
        edgeCoord.x = srcCoord.x;
        edgeCoord.y = srcCoord.y / 2 - 1;
        upperChromaEdge =
          intel_sub_group_media_block_read_us(
                                    edgeCoord,
                                    8,
                                    1,
                                    intra_src_chroma_image );
      }
    }

    int countSkipMVs = count.y;

    uint2 skipMV = 0;
    bool is8x8 = false;

    if( countSkipMVs != 0 ) {	  
      uint offset = curMB * 8;  // 8 sets of skip check MVs/predictors per MB

	  offset += get_local_id(0) % 8;           // 16 work-items access 8 MVs for MB
                                               // one predictor for MB per SIMD channel

      // Reduce predictors from QPEL to PEL resolution.

      int2 predMV = 0;

      if( get_local_id(0) < countPredMVs ) {
        predMV = convert_int2( prediction_motion_vector_buffer [ offset ] );
        refCoords.x = predMV.x / 4;                                             
        refCoords.y = predMV.y / 4;                                                       
        refCoords.y = refCoords.y & 0xFFFE;
      }

      if( skip_block_type == 0x0 ) {
        __global uint *skip1_motion_vector_buffer = ( __global uint * ) skip_motion_vector_buffer;
        offset += ( get_local_id(0) % 8 );
        skipMV.s0 = skip1_motion_vector_buffer[ offset ];
        skipMV.s1 = skipMV.s0;
        is8x8 = false;
      }
      else {
        __global uint4 *skip4_motion_vector_buffer = ( __global uint4 * ) ( skip_motion_vector_buffer );
        skipMV = intel_sub_group_block_read2( ( __global uint* ) ( skip4_motion_vector_buffer + offset ) );
        is8x8 = true;
      }
    }

    if( ( countSkipMVs != 0 ) | ( doIntra ) ) {
      ushort udists[ 3 ] = {0, 0, 0};
      ulong  umodes[ 3 ] = {0, 0, 0};
      ushort ucdist = 0;
      uchar  ucmode = 0;

      ushort distsH1 = 0;

      bool doIntraMask[ 4 ] = { true, true, true, false };
      uchar intraPartMasks[ 4 ] = {
        CLK_AVC_ME_INTRA_LUMA_PARTITION_MASK_16x16_INTEL,
        CLK_AVC_ME_INTRA_LUMA_PARTITION_MASK_8x8_INTEL,
        CLK_AVC_ME_INTRA_LUMA_PARTITION_MASK_4x4_INTEL,
        0 };

      for( int j = 0; j < 4; j++ ) {
        bool doSKC = countSkipMVs > j;
        if( ( doSKC ) | ( doIntra & doIntraMask[ j ] ) ) {
          intel_sub_group_avc_sic_result_t result;
          result =
            sic_eval(
                     accelerator,
                     srcImg,                              
                     refImg,
                     countSkipMVs > j,
                     doIntra & doIntraMask[ j ],
					 doChromaIntra & doIntraMask[ j ],
                     is8x8,
                     intraPartMasks[ j ],
                     intraEdges,
                     leftLumaEdge,
                     leftUpperLumaPixel,
                     upperLumaEdge,
                     upperRightLumaEdge,
                     leftChromaEdge,
                     leftUpperChromaPixel,
                     upperChromaEdge,
                     srcCoord,
                     sad_adjustment,
					 refCoords,
                     skipMV.s0,
                     j,
                     pixel_mode,
					 search_cost_precision,
					 search_cost_penalty );
  
          if( doSKC ) {
            ushort udists = intel_sub_group_avc_sic_get_inter_distortions( result );
            distsH1 = extract_distortion_h1( udists, is8x8, j, distsH1 );
          }
          if( doIntra & doIntraMask[ j ] ) {
            udists[ j ] = intel_sub_group_avc_sic_get_best_ipe_luma_distortion( result );
            umodes[ j ] = intel_sub_group_avc_sic_get_packed_ipe_luma_modes( result );
			if( doChromaIntra & doIntraMask[ j ] ) {
				ucdist  = intel_sub_group_avc_sic_get_best_ipe_chroma_distortion( result );
				ucmode  = intel_sub_group_avc_sic_get_ipe_chroma_mode( result );
			}
          }
        }
      }

      ushort distsH2 = 0;

      for( int j = 4; j < 8; j++ ) {
        if( countSkipMVs > j ) {
          intel_sub_group_avc_sic_result_t result;
          result =
            sic_eval(
                     accelerator,
                     srcImg,                              
                     refImg,      
                     true,
                     false,
					 false,
                     is8x8,
                     0,
                     0,
                     0,
                     0,
                     0,
                     0,
                     0,
                     0,
                     0,
                     srcCoord,
                     sad_adjustment,
					 refCoords,
                     skipMV.s1,
                     j + 4,
                     pixel_mode,
					 search_cost_precision,
					 search_cost_penalty );
          ushort udists = intel_sub_group_avc_sic_get_inter_distortions( result );
          if( is8x8 ) {
            distsH2 = extract_distortion_h2( udists, is8x8, j, distsH2 );
          }
          else {
            distsH1 = extract_distortion_h2( udists, is8x8, j, distsH1 );
          }
        }
      }

      if( countSkipMVs ) {

        // Write out motion skip check result:
        // Result format
        //     Hierarchical row-major layout
        //     i.e. row-major of blocks in MBs, and row-major of 8 sets of distortions in blocks

        if( skip_block_type == 0x0 ) {
          // Copy out 8 (1 component) sets of distortion values.

          int index =
            ( gid_0 * 8 ) + ( get_local_id(0) ) +
            ( gid_1 * 8 * get_num_groups(0) );

          if( get_local_id(0) < countSkipMVs ) {
            skip_residuals [ index ] = distsH1;
          }
        }
        else {
          // Copy out 8 (4 component) sets of distortion values.

          int index =
            ( gid_0 * 8 * 4 ) + ( get_local_id(0) ) +
            ( gid_1 * 8 * 4 * get_num_groups(0) );

          if( get_local_id(0) < countSkipMVs * 4 ) {
            skip_residuals [ index ] = distsH1;
            skip_residuals [ index + 16 ] = distsH2;
          }
        }
      }

      // Write out intra search result:

      if( doIntra) {

        // Write out the 4x4 intra modes
        if( get_local_id(0) < 16 ) {
          uint shiftCount = get_local_id( 0 ) * 4;
          char value = ( umodes[ 2 ] >> shiftCount ) & 0xF;
          int index =
            ( gid_0 * 22 ) + ( get_local_id(0) ) +
            ( gid_1 * 22 * get_num_groups(0) );
          intra_search_predictor_modes[ index + 5 ] = value;
        }

        if( get_local_id(0) < 4 ) {
          uint shiftCount = get_local_id( 0 ) * 16;
          char value = ( umodes[ 1 ] >> shiftCount ) & 0xF;
          int index =
            ( gid_0 * 22 ) + ( get_local_id(0) ) +
            ( gid_1 * 22 * get_num_groups(0) );
          intra_search_predictor_modes[ index + 1 ] = value;
        }

        // Write out the 16x16 intra modes
        if( get_local_id(0) < 1 ) {
          char value = umodes[ 0 ] & 0xF;
          int index =
            ( gid_0 * 22 ) + ( get_local_id(0) ) +
            ( gid_1 * 22 * get_num_groups(0) );
          intra_search_predictor_modes[ index ] = value;
        }

        // Write out the 8x8 intra chroma mode
        if( ( doChromaIntra ) && ( get_local_id(0) < 1 ) ) {
          char value = ucmode & 0xF;
          int index =
            ( gid_0 * 22 ) + ( get_local_id(0) ) +
            ( gid_1 * 22 * get_num_groups(0) );
          intra_search_predictor_modes[ index + 21 ] = value;
        }

        // Get the intra residuals.
        if( intra_residuals  != NULL )
        {
            int index =
              ( gid_0 * 4 ) +
              ( gid_1 * 4 * get_num_groups(0) );

            if( get_local_id(0) < 1 ) {
			  if( doChromaIntra ) {	
				intra_residuals[ index + 3 ] = ucdist;
			  }
              intra_residuals[ index + 2 ] = udists[ 2 ];
              intra_residuals[ index + 1 ] = udists[ 1 ];
              intra_residuals[ index + 0 ] = udists[ 0 ];
            }
        }
      }
    }
  }
}
