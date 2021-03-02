!$Id: integrate.F90 1565 2015-02-28 17:04:41Z lnerger $
!BOP
!
! !ROUTINE: integrate --- Time stepping loop of tutorial model
!
! !INTERFACE:
SUBROUTINE integrate()

! !DESCRIPTION:
! Initialization routine for the simple 2D model without
! parallelization of the model.
!
! The routine defines the size of the model grid and
! read the initial state from a file.
!
! !REVISION HISTORY:
! 2013-09 - Lars Nerger - Initial code based on offline_1D
! Later revisions - see svn log
!
! !USES:
  USE mod_model, &
       ONLY: ny, nx_p, field_p, melissa_field_name
  USE mod_parallel_model, &
       ONLY: mype_world

  USE iso_c_binding

  IMPLICIT NONE

  INCLUDE 'melissa_da_api.f90'



! !CALLING SEQUENCE:
! Called by: main
!EOP

! *** local variables ***
  INTEGER :: step, i, j, counter        ! Counters
  REAL :: store                ! Store single field element

  REAL(kind=C_DOUBLE) :: field_double(nx_p * ny)

  INTEGER :: nsteps = 1    ! if not 0 we are timestepping.



! ****************
! *** STEPPING ***
! ****************

  IF (mype_world==0) WRITE (*, '(1x, a)') 'START INTEGRATION'


  stepping: DO WHILE (nsteps > 0)
     WRITE (*, *) 'integrating how many timesteps?', nsteps
     DO step = 1, nsteps
         IF (mype_world==0) WRITE (*,*) 'step', step

    ! *** Time step: Shift field vertically ***
         DO j = 1, nx_p
            store = field_p(ny, j)

            DO i = ny-1,1,-1
               field_p(i+1, j) = field_p(i, j)
            END DO

            field_p(1, j) = store

         END DO
     END DO


     counter = 1
     DO j = 1, nx_p
       DO i = 1, ny
         field_double(counter) = field_p(i,j)
         counter = counter + 1
       END DO
     END DO

     nsteps = melissa_expose(melissa_field_name, field_double)

     counter = 1
     DO j = 1, nx_p
       DO i = 1, ny
         field_p(i,j) = field_double(counter)
         counter = counter + 1
       END DO
     END DO

! *** Write new field into file ***

     ! Gather global field on process 0
     !ALLOCATE(field(ny, nx))

     !CALL MPI_Gather(field_p, nx_p*ny, MPI_DOUBLE_PRECISION, field, nx_p*ny, &
     !     MPI_DOUBLE_PRECISION, 0, COMM_model, MPIerr)

     ! Write file from process 0 ! we let the server write!
     !IF (mype_world==0) THEN
     !   WRITE (stepstr, '(i2.2)') step
     !   OPEN(11, file = 'true_step'//TRIM(stepstr)//'.txt', status = 'replace')

     !   DO i = 1, ny
     !      WRITE (11, *) field(i, :)
     !   END DO

     !   CLOSE(11)
     !END IF

     !DEALLOCATE(field)

  END DO stepping

END SUBROUTINE integrate
