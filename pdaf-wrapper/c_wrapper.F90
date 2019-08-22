! TODO: take the dummy model or maybe even others to init parallel!
SUBROUTINE cwrapper_init_pdaf(param_dim_state, param_ensemble_size, param_total_steps) BIND(C,name='cwrapper_init_pdaf')
  USE iso_c_binding

  IMPLICIT NONE

  INTEGER(kind=C_INT), intent(in) :: param_dim_state
  INTEGER(kind=C_INT), intent(in) :: param_ensemble_size
  INTEGER(kind=C_INT), intent(in) :: param_total_steps

  ! Revise parallelization for ensemble assimilation
  CALL init_parallel_pdaf(0, 1)


! TODO: dim_state and the other things that require initialize must be parameters!,  see used variables in initialize.f90

  ! Initialize PDAF
  CALL init_pdaf()

  ! TODO: also init parallel
END SUBROUTINE


SUBROUTINE cwrapper_PDAF_deallocate() BIND(C,name='cwrapper_PDAF_deallocate')
  USE iso_c_binding

  IMPLICIT NONE

  CALL finalize_pdaf()

END SUBROUTINE



module my_state_accessors
use iso_c_binding
implicit none
save
real(C_DOUBLE), POINTER :: distribute_state_to(:)
real(C_DOUBLE), POINTER :: collect_state_from(:)
end module

SUBROUTINE my_distribute_state(dim, state)
  USE my_state_accessors, &
    only: distribute_state_to

  IMPLICIT NONE

! !ARGUMENTS:
  INTEGER, INTENT(in) :: dim         ! State dimension
  REAL, INTENT(inout) :: state(dim)  ! State vector

  distribute_state_to(:) = state(:)

END SUBROUTINE my_distribute_state

SUBROUTINE my_collect_state(dim, state)

  USE my_state_accessors, &
       ONLY: collect_state_from

  IMPLICIT NONE

! !ARGUMENTS:
  INTEGER, INTENT(in) :: dim           ! PE-local state dimension
  REAL, INTENT(inout) :: state(dim)  ! local state vector


  state(:) = collect_state_from(:)

END SUBROUTINE my_collect_state


SUBROUTINE cwrapper_PDAF_get_state(doexit, dim_state_analysis, state_analysis, status) BIND(C, name='cwrapper_PDAF_get_state')
  use iso_c_binding
  USE my_state_accessors, &
  only: distribute_state_to


  IMPLICIT NONE

! Arguments:
  INTEGER(C_INT), intent(out) :: doexit
  INTEGER(C_INT), intent(in) :: dim_state_analysis
  TYPE(C_PTR) :: state_analysis
  INTEGER(C_INT), intent(out) :: status

! ! External subroutines
! !  (subroutine names are passed over to PDAF in the calls to
! !  PDAF_get_state and PDAF_put_state_X. This allows the user
! !  to specify the actual name of a routine. However, the
! !  PDAF-internal name of a subroutine might be different from
! !  the external name!)
!
! ! Subroutines used with all filters
  EXTERNAL :: next_observation, & ! Provide time step, model time, &
                                  ! and dimension of next observation
       my_distribute_state, &        ! Routine to distribute a state vector to model fields
       collect_state, &           ! Routine to collect a state vector from model fields
       init_dim_obs, &            ! Initialize dimension of observation vector
       obs_op, &                  ! Implementation of the Observation operator
       init_obs, &                ! Routine to provide vector of measurements
       distribute_stateinc        ! Routine to add state increment for IAU
! ! Subroutine used in SEIK
  EXTERNAL :: prepoststep_seik, & ! User supplied pre/poststep routine for SEIK
       init_obsvar                ! Initialize mean observation error variance
! ! Subroutine used in SEIK and SEEK
  EXTERNAL :: prodRinvA           ! Provide product R^-1 A for some matrix A
! ! Subroutines used in EnKF
  EXTERNAL :: add_obs_error, &    ! Add obs. error covariance R to HPH in EnKF
       init_obscovar              ! Initialize obs error covar R in EnKF
! ! Subroutines used in LSEIK
  EXTERNAL :: init_n_domains, &   ! Provide number of local analysis domains
       init_dim_local, &      ! Initialize state dimension for local ana. domain
       init_dim_obs_local,&   ! Initialize dim. of obs. vector for local ana. domain
       global2local_state, &  ! Get state on local ana. domain from global state
       local2global_state, &  ! Init global state from state on local analysis domain
       global2local_obs, &    ! Restrict a global obs. vector to local analysis domain
       init_obs_local, &      ! Provide vector of measurements for local ana. domain
       prodRinvA_local, &     ! Provide product R^-1 A for some matrix A (for LSEIK)
       init_obsvar_local, &   ! Initialize local mean observation error variance
       init_obs_full, &       ! Provide full vector of measurements for PE-local domain
       obs_op_full, &         ! Obs. operator for full obs. vector for PE-local domain
       init_dim_obs_full      ! Get dimension of full obs. vector for PE-local domain
! ! Subroutines used in NETF
  EXTERNAL :: likelihood      ! Compute observation likelihood for an ensemble member
! ! Subroutines used in LNETF
  EXTERNAL :: likelihood_local  ! Compute local observation likelihood for an ensemble member



! local variables
  INTEGER :: nsteps    ! Number of time steps to be performed in current forecast
  REAL :: timenow      ! Current model time
  !REAL :: time         ! Model time  TODO: needed?


  Print *, "Hello get state!"



  ! TODO: set pointer...
  !distribute_state_to => state_analysis
  CALL C_F_POINTER( state_analysis, distribute_state_to,[dim_state_analysis])
  CALL PDAF_get_state(nsteps, timenow, doexit, next_observation, &
          my_distribute_state, prepoststep_seik, status)

END SUBROUTINE

SUBROUTINE cwrapper_PDFA_put_state(state_background, status) BIND(C, name='cwrapper_PDFA_put_state')
  USE iso_c_binding

  USE my_state_accessors, &
       ONLY: collect_state_from

  IMPLICIT NONE

! Arguments
  INTEGER(C_INT) :: dim_state_background
  TYPE(C_PTR) :: state_background
  INTEGER(C_INT), intent(out) :: status

! ! External subroutines
! !  (subroutine names are passed over to PDAF in the calls to
! !  PDAF_get_state and PDAF_put_state_X. This allows the user
! !  to specify the actual name of a routine. However, the
! !  PDAF-internal name of a subroutine might be different from
! !  the external name!)
!
! ! Subroutines used with all filters
  EXTERNAL :: next_observation, & ! Provide time step, model time, &
                                  ! and dimension of next observation
       distribute_state, &        ! Routine to distribute a state vector to model fields
       collect_state, &           ! Routine to collect a state vector from model fields
       init_dim_obs, &            ! Initialize dimension of observation vector
       obs_op, &                  ! Implementation of the Observation operator
       init_obs, &                ! Routine to provide vector of measurements
       distribute_stateinc        ! Routine to add state increment for IAU
! ! Subroutine used in SEIK
  EXTERNAL :: prepoststep_seik, & ! User supplied pre/poststep routine for SEIK
       init_obsvar                ! Initialize mean observation error variance
! ! Subroutine used in SEIK and SEEK
  EXTERNAL :: prodRinvA           ! Provide product R^-1 A for some matrix A
! ! Subroutines used in EnKF
  EXTERNAL :: add_obs_error, &    ! Add obs. error covariance R to HPH in EnKF
       init_obscovar              ! Initialize obs error covar R in EnKF
! ! Subroutines used in LSEIK
  EXTERNAL :: init_n_domains, &   ! Provide number of local analysis domains
       init_dim_local, &      ! Initialize state dimension for local ana. domain
       init_dim_obs_local,&   ! Initialize dim. of obs. vector for local ana. domain
       global2local_state, &  ! Get state on local ana. domain from global state
       local2global_state, &  ! Init global state from state on local analysis domain
       global2local_obs, &    ! Restrict a global obs. vector to local analysis domain
       init_obs_local, &      ! Provide vector of measurements for local ana. domain
       prodRinvA_local, &     ! Provide product R^-1 A for some matrix A (for LSEIK)
       init_obsvar_local, &   ! Initialize local mean observation error variance
       init_obs_full, &       ! Provide full vector of measurements for PE-local domain
       obs_op_full, &         ! Obs. operator for full obs. vector for PE-local domain
       init_dim_obs_full      ! Get dimension of full obs. vector for PE-local domain
! ! Subroutines used in NETF
  EXTERNAL :: likelihood      ! Compute observation likelihood for an ensemble member
! ! Subroutines used in LNETF
  EXTERNAL :: likelihood_local  ! Compute local observation likelihood for an ensemble member

  Print *, "Hello put state!"
  !collect_state_from => state_background
  CALL C_F_POINTER( state_background, collect_state_from,[dim_state_background])
  CALL PDAF_put_state_enkf(collect_state, init_dim_obs, obs_op, &
           init_obs, prepoststep_seik, add_obs_error, &
           init_obscovar, status)


END SUBROUTINE
