!$Id: initialize.F90 1565 2015-02-28 17:04:41Z lnerger $
!BOP
!
! !ROUTINE: initialize --- Initialize model
!
! !INTERFACE:
SUBROUTINE initialize()

! !DESCRIPTION:
! Initialization routine for the simple 2D model without
! parallelization of the model.
!
! The routine defines the size of the model grid and
! read the initial state from a file.
!
! !REVISION HISTORY:
! 2013-09 - Lars Nerger - Initial code
! Later revisions - see svn log
!
! !USES:
  USE mod_model, &
       ONLY: nx, ny, nx_p, field_p, melissa_field_name, init_nxy
  USE mod_parallel_model, &
       ONLY: mype_world, mype_model, npes_model, abort_parallel, MPI_COMM_WORLD

  USE iso_c_binding

  IMPLICIT NONE

  INCLUDE 'melissa_da_api.f90'


! !CALLING SEQUENCE:
! Called by: main
!EOP

! *** local variables ***
  INTEGER :: i, j                 ! Counters
  REAL, ALLOCATABLE :: field(:,:) ! GLobal model field
  CHARACTER(len=256) :: dataset_path     ! pdaf path, load from environment variable




! **********************
! *** INITIALIZATION ***
! **********************

  call init_nxy(npes_model)
! *** Model specifications ***

! *** Screen output ***
  IF (mype_world == 0) THEN
     WRITE (*, '(1x, a)') 'INITIALIZE PARALLELIZED 2D TUTORIAL MODEL'
     WRITE (*, '(10x,a,i4,1x,a1,1x,i4)') 'Grid size:', nx, 'x', ny
     WRITE (*, '(/2x, a, i3, a)') &
          '-- Domain decomposition over', npes_model, ' PEs'
     WRITE (*, '(2x,a,i3,a,i3)') &
          '-- local domain sizes (nx_p x ny): ', nx_p, ' x', ny
  END IF

  ! allocate memory for process-local part of field
  ALLOCATE(field_p(ny, nx_p))


! ************************************
! *** Read initial field from file ***
! ************************************


  if (nx == 36 .and. ny == 18) then
      ALLOCATE(field(ny, nx))
      ! Read global model field
      call get_environment_variable( 'MELISSA_DA_DATASET_PATH', dataset_path )
      OPEN(11, file = TRIM(dataset_path)//'/true_initial.txt', status='old')

      DO i = 1, ny
         READ (11, *) field(i, :)
      END DO

      CLOSE(11)

      ! Initialize local part of model field
      DO j = 1, nx_p
         DO i = 1, ny
            field_p(i,j) = field(i, nx_p*mype_model + j)
         END DO
      END DO

      DEALLOCATE(field)
  else
      DO j = 1, nx_p
         DO i = 1, ny
            field_p(i,j) = 1.0
            if (i == nx/2) then
                field_p(i,j) = 1.1
            end if
         END DO
      END DO


  end if

  CALL MELISSA_INIT_F(melissa_field_name, nx_p*ny, 0, MPI_COMM_WORLD)


END SUBROUTINE initialize
