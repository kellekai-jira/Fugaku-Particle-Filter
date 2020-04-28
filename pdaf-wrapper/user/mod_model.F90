!$Id: mod_model.F90 1411 2013-09-25 14:04:41Z lnerger $
!BOP
!
! !MODULE:
MODULE mod_model

! !DESCRIPTION:
! This module provides variables needed for the
! 2-dimensional tutorial model without parallelization.
!
! !REVISION HISTORY:
! 2013-09 - Lars Nerger - Initial code
! Later revisions - see svn log
!
! !USES:
use iso_c_binding
  IMPLICIT NONE
  SAVE
!EOP


! *** Variables specific for 2D tutorial model ***

  INTEGER :: total_steps
  INTEGER :: nx, ny               ! Size of 2D grid
  REAL, ALLOCATABLE :: field_p(:,:) ! Decomposed model field

  INTEGER :: nx_p                 ! Process-local size in x-direction
  CHARACTER(len=6) :: melissa_field_name = C_CHAR_'state'//C_NULL_CHAR
  contains
      elemental subroutine str_to_int(str,int,stat)
          implicit none
          ! Arguments
          character(len=*),intent(in) :: str
          integer,intent(out)         :: int
          integer,intent(out)         :: stat

          read(str,*,iostat=stat)  int
      end subroutine
      subroutine init_nxy(npes)
          IMPLICIT NONE
          integer, intent(in) :: npes
          CHARACTER(len=256) :: NX_str     ! NX env string
          CHARACTER(len=256) :: NY_str     ! NY env string
          INTEGER :: status


          call get_environment_variable("NX", NX_str)
          call get_environment_variable("NY", NY_str)
          nx = 36          ! Extent of grid in x-direction
          ny = 18          ! Extent of grid in y-direction

          call str_to_int(NX_Str, nx, status)
          if (status == 0) then
              nx = 36
          end if
          call str_to_int(NY_Str, ny, status)
          if (status == 0) then
              ny = 18
          end if

          ! Split x-diection in chunks of equal size
          if (modulo(nx, npes) /= 0) THEN
          WRITE (*,*) 'ERROR: Invalid number of processes! cannot divide', NX, 'by', npes, 'processes'
              CALL exit(1)
          END IF
          nx_p = nx / npes

      end subroutine

END MODULE mod_model
