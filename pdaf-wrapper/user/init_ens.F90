!-------------------------------------------------------------------------------------------
!Copyright (c) 2013-2016 by Wolfgang Kurtz and Guowei He (Forschungszentrum Juelich GmbH)
!
!This file is part of TerrSysMP-PDAF
!
!TerrSysMP-PDAF is free software: you can redistribute it and/or modify
!it under the terms of the GNU Lesser General Public License as published by
!the Free Software Foundation, either version 3 of the License, or
!(at your option) any later version.
!
!TerrSysMP-PDAF is distributed in the hope that it will be useful,
!but WITHOUT ANY WARRANTY; without even the implied warranty of
!MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
!GNU LesserGeneral Public License for more details.
!
!You should have received a copy of the GNU Lesser General Public License
!along with TerrSysMP-PDAF.  If not, see <http://www.gnu.org/licenses/>.
!-------------------------------------------------------------------------------------------
!
!
!-------------------------------------------------------------------------------------------
!init_ens.F90: TerrSysMP-PDAF implementation of routine
!              'init_ens' (PDAF online coupling)
!-------------------------------------------------------------------------------------------

!$Id: init_ens.F90 1444 2013-10-04 10:54:08Z lnerger $
!BOP
!
! !ROUTINE: init_ens --- Initialize ensemble
!
! !INTERFACE:
SUBROUTINE init_ens(filtertype, dim_p, dim_ens, state_p, Uinv, &
    ens_p, flag)

    ! !DESCRIPTION:
    ! User-supplied routine for PDAF.
    ! Used in the filters: SEIK/LSEIK/ETKF/LETKF/ESTKF/LESTKF
    !
    ! The routine is called when the filter is
    ! initialized in PDAF\_filter\_init.  It has
    ! to initialize an ensemble of dim\_ens states.
    ! Typically, the ensemble will be directly read from files.
    !
    ! The routine is called by all filter processes and
    ! initializes the ensemble for the PE-local domain.
    !
    ! !REVISION HISTORY:
    ! 2013-02 - Lars Nerger - Initial code
    ! Later revisions - see svn log
    !
    ! !USES:
    !   USE mod_model, &
    !        ONLY: nx, ny, nx_p
    USE mod_parallel_model, &
        ONLY: mype_model, model, &
        mype_world
    USE mod_parallel_pdaf, &
        ONLY: mype_filter, task_id
    use mod_tsmp, &
        only: tag_model_parflow, pf_statevecsize, pf_statevec, pf_statevec_fortran
    use netcdf

    IMPLICIT NONE

    ! !ARGUMENTS:
    INTEGER, INTENT(in) :: filtertype              ! Type of filter to initialize
    INTEGER, INTENT(in) :: dim_p                   ! PE-local state dimension
    INTEGER, INTENT(in) :: dim_ens                 ! Size of ensemble
    REAL, INTENT(inout) :: state_p(dim_p)          ! PE-local model state
    ! It is not necessary to initialize the array 'state_p' for SEIK.
    ! It is available here only for convenience and can be used freely.
    REAL, INTENT(inout) :: Uinv(dim_ens-1,dim_ens-1) ! Array not referenced for SEIK
    REAL, INTENT(out)   :: ens_p(dim_p, dim_ens)   ! PE-local state ensemble
    INTEGER, INTENT(inout) :: flag                 ! PDAF status flag
    ! !CALLING SEQUENCE:
    ! Called by: PDAF_filter_init    (as U_ens_init)
    !EOP

    ! *** local variables ***
    INTEGER :: i, j, member  ! Counters
    INTEGER :: dimid_t, dimid_x, dimid_y, dimid_z  ! dim ids
    INTEGER :: dim_t, dim_x, dim_y, dim_z  ! dims
    INTEGER :: pres_varid, ncid  ! other id's
    INTEGER, DIMENSION(1:4) :: start  ! start of the slice we want to read from the netcdf file
    INTEGER, DIMENSION(1:4) :: ct  ! count/size of the slice we want to read from the netcdf file

    !REAL, DIMENSION(1:1, 1:12, 1:50, 1:25) :: tmp ! just for testing dimensions
    REAL, DIMENSION(1:1500) :: tmp ! just for testing dimensions

    character(len = nf90_max_name) :: RecordDimName


    !TODO: Write testcase to check that dimensions are good!

    ! **********************
    ! *** INITIALIZATION ***
    ! **********************

    ! *** Generate full ensemble on filter-PE 0 ***
    IF (mype_filter==0) THEN
        WRITE (*, '(/9x, a)') 'Initialize state ensemble'
        WRITE (*, '(9x, a)') '--- read ensemble from files'
        WRITE (*, '(9x, a, i5)') '--- Ensemble size:  ', dim_ens
    END IF



    ! ********************************
    ! *** Read ensemble from files ***
    ! ********************************

    !    WRITE (*,*) 'TEMPLATE init_ens.F90: Initialize ensemble array ens_p!'

    !    convert pf_statevec to fortran pointer

    ! Normally this is initialized by random values as the ensemble members start from the pressure map as specified in the
    ! different tcl files. we don't do this as there is only one tcl file per runner but multiple ensemble members per runner...
    !if (model == tag_model_parflow) then
        !!print *, "Parflow component: initialize ensemble array ens_p"
        !!print *, "my dim_p is", dim_p
        !do i = 1, dim_ens
            !ens_p(:, i) = 10 + mype_model + i
        !end do
    !else
        !do i = 1, dim_ens
            !ens_p(:, i) = 1.1
        !end do
        !!print *, "CLM component: initialize ensemble array ens_p"
        !!print *, "my dim_p is", dim_p

    !end if
    ! TODO: here is a static filename for now... path realtive to server cwd
    call check( nf90_open("./press.nc", nf90_nowrite, ncid) )
    !call check( nf90_open("./p2.nc", nf90_nowrite, ncid) )


    call check(nf90_inq_dimid(ncid, "longitude",     dimid_x))
    call check(nf90_inq_dimid(ncid, "latitude",     dimid_y))
    call check(nf90_inq_dimid(ncid, "depth", dimid_z))
    call check(nf90_inq_dimid(ncid, "time", dimid_t))

    call check(nf90_inquire_dimension(ncid, dimid_x, recorddimname, dim_x))
    call check(nf90_inquire_dimension(ncid, dimid_y, recorddimname, dim_y))
    call check(nf90_inquire_dimension(ncid, dimid_z, recorddimname, dim_z))
    call check(nf90_inquire_dimension(ncid, dimid_t, recorddimname, dim_t))
    print *, "dim_x = ", dim_x
    print *, "dim_y = ", dim_y
    print *, "dim_z = ", dim_z
    print *, "dim_t = ", dim_t
    print *,  (dim_x*dim_y*dim_z), " == ", (50*50*12), "?"

    call check( nf90_inq_varid(ncid, "press", pres_varid) )

    do i = 1, dim_ens
        ens_p(:, i) = 10

        ! but is x y z t
        ! is double checked and works like this with parflow. TODO a testcase would still be
        ! nice!
        print *, "i = ", i, ", mype_world = ", mype_world
        start = (/ 1+25*mype_world, 1, 1, i /)  ! start i+100 timesteps later?
        ct = (/ 25, 50, 12, 1 /)
        !call check(nf90_get_var(ncid, pres_varid, tmp, &
            !start, &
            !ct))
        call check(nf90_get_var(ncid, pres_varid, ens_p(:, i), &
            start, &
            ct))
    end do

    call check( nf90_close(ncid) )

! ****************
! *** clean up ***
! ****************


END SUBROUTINE init_ens
