!$Id: init_dim_obs_pdaf.F90 1864 2017-12-20 19:53:30Z lnerger $
!BOP
!
! !ROUTINE: init_dim_obs_pdaf --- Compute number of observations
!
! !INTERFACE:
SUBROUTINE init_dim_obs_pdaf(step, dim_obs_p)

! !DESCRIPTION:
! User-supplied routine for PDAF.
! Used in the filters: SEEK/SEIK/EnKF/ETKF/ESTKF
!
! The routine is called at the beginning of each
! analysis step.  It has to initialize the size of
! the observation vector according to the current
! time step for the PE-local domain.
!
! Implementation for the 2D online example
! without parallelization.
!
! !REVISION HISTORY:
! 2013-02 - Lars Nerger - Initial code
! Later revisions - see svn log
!
! !USES:
  USE mod_assimilation, &
       ONLY: obs_p, obs_index_p
  USE mod_model, &
       ONLY: nx, ny, nx_p
  USE mod_parallel_pdaf, &
       ONLY: mype_filter
  USE my_state_accessors, &
      ONLY: current_step

#ifdef __INTEL_COMPILER
  USE ifport
#endif

  IMPLICIT NONE

! !ARGUMENTS:
  INTEGER, INTENT(in)  :: step      ! Current time step
  INTEGER, INTENT(out) :: dim_obs_p ! Dimension of observation vector

! !CALLING SEQUENCE:
! Called by: PDAF_seek_analysis    (as U_init_dim_obs)
! Called by: PDAF_seik_analysis, PDAF_seik_analysis_newT
! Called by: PDAF_enkf_analysis_rlm, PDAF_enkf_analysis_rsm
! Called by: PDAF_etkf_analysis, PDAF_etkf_analysis_T
! Called by: PDAF_estkf_analysis, PDAF_estkf_analysis_fixed
!EOP

! *** Local variables
  INTEGER :: i, j                     ! Counters
  INTEGER :: cnt0, cnt_p, cnt0_p      ! Counters
  INTEGER :: off_p                    ! process-local offset in state vector
  REAL, ALLOCATABLE :: obs_field(:,:) ! Array for observation field read from file
  CHARACTER(len=2) :: stepstr         ! String for time step
  CHARACTER(len=256) :: dataset_path     ! pdaf path, load from environment variable


! ****************************************
! *** Initialize observation dimension ***
! ****************************************

  ! Determine offset in state vector for this process
  off_p = 0
  DO i= 1, mype_filter
     off_p = off_p + nx_p*ny
  END DO

  ! Read observation field form file
  ALLOCATE(obs_field(ny, nx))

  print *, "NXY:", nx,ny
  if (nx == 36 .and. ny == 18) then
     IF (current_step<10) THEN
        WRITE (stepstr, '(i1)') current_step
     ELSE
        WRITE (stepstr, '(i2)') current_step
     END IF

     call get_environment_variable( 'DATASET_PATH', dataset_path )
     OPEN (12, &
       file=TRIM(dataset_path)//'/obs_step'// &
       TRIM(stepstr)//'.txt', status='old')
     DO i = 1, ny
        READ (12, *) obs_field(i, :)
     END DO
     CLOSE (12)
  else
    do j = 1, ny
      do i = 1, nx
        if (modulo(i, 2) == 0) then
          obs_field(j,i) = 0.9*rand()-0.45
        else
          obs_field(j,i) = -1000.0
        end if
      end do
    end do
  end if

  ! Count observations
  cnt0 = 0
  cnt_p = 0
  DO j = 1, nx
     DO i= 1, ny
        cnt0 = cnt0 + 1
        IF (cnt0 > off_p .AND. cnt0 <= off_p + nx_p*ny) THEN
           IF (obs_field(i,j) > -999.0) cnt_p = cnt_p + 1
        END IF
     END DO
  END DO

  ! Set number of observations
  dim_obs_p = cnt_p

  ! Initialize vector of observations and index array
  IF (ALLOCATED(obs_index_p)) DEALLOCATE(obs_index_p)
  IF (ALLOCATED(obs_p)) DEALLOCATE(obs_p)
  ALLOCATE(obs_index_p(dim_obs_p))
  ALLOCATE(obs_p(dim_obs_p))

  cnt0 = 0
  cnt_p = 0
  cnt0_p = 0
  DO j = 1, nx
     DO i= 1, ny
        cnt0 = cnt0 + 1
        IF (cnt0 > off_p .AND. cnt0 <= off_p + nx_p*ny) THEN
           cnt0_p = cnt0_p + 1
           IF (obs_field(i,j) > -999.0) THEN
              cnt_p = cnt_p + 1
              obs_index_p(cnt_p) = cnt0_p
              obs_p(cnt_p) = obs_field(i, j)
           END IF
        END IF
     END DO
  END DO


! *** Clean up ***

  DEALLOCATE(obs_field)

END SUBROUTINE init_dim_obs_pdaf

