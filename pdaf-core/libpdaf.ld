PDAF {
  global:
    pdaf_gather_dim_obs_f_;
    pdaf_local_weight_;
    pdaf_gather_obs_f2_;
    pdaf_put_state_estkf_;
    pdaf_get_state_;
    pdaf_put_state_enkf_;
    pdaf_print_info_;
    pdaf_gather_obs_f_;
    pdaf_deallocate_;
    pdaf_init_;
  local: *;
};
