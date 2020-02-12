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
  INTEGER :: nx, ny, nz               ! Size of 2D grid
!  REAL, ALLOCATABLE :: field_p(:,:) ! Decomposed model field

  INTEGER :: nx_p                 ! Process-local size in x-direction

  ! global variables for init_ens_hidden
  INTEGER :: dimid_t, dimid_x, dimid_y, dimid_z  ! dim ids
  INTEGER :: dim_t, dim_x, dim_y, dim_z  ! dims
  INTEGER :: dens_varid, satur_varid, ncid ! other id's

END MODULE mod_model
