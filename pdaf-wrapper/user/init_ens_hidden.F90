!BOP
!
! !ROUTINE: init_ens_hidden --- Initialize ensemble members hidden state, state that is not assimilated but needed to restart
! model...
!
! !INTERFACE:
SUBROUTINE init_ens_hidden(dim_p, dim_ens, member_id, hidden_state_p)
    ! !DESCRIPTION:
    ! User-supplied routine for Melissa-DA with PDAF.
    ! Used in the filters: SEIK/LSEIK/ETKF/LETKF/ESTKF/LESTKF
    !
    ! The routine is called when the filter is
    ! initialized in the constructor of the PDAF assimilator.  It has
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
    USE mod_parallel_model, &
        ONLY: mype_model, &
        mype_world
    USE mod_parallel_pdaf, &
        ONLY: mype_filter, task_id
    !use mod_tsmp, &
        !only: tag_model_parflow, pf_statevecsize, pf_statevec, pf_statevec_fortran
    USE mod_model, &
        ONLY: dimid_t, dimid_x, dimid_y, dimid_z, &
              dim_t, dim_x, dim_y, dim_z, &
              dens_varid, satur_varid, ncid
    use netcdf

    IMPLICIT NONE

    ! !ARGUMENTS:
    INTEGER, INTENT(in) :: dim_p                   ! PE-local state dimension
    INTEGER, INTENT(in) :: dim_ens                 ! Size of ensemble
    INTEGER, INTENT(in) :: member_id               ! id of the member that shall be initialized by this function. starts at 0 and
    ! goes up to dim_ens - 1
    REAL, INTENT(out)   :: hidden_state_p(dim_p)            ! PE-local state ensemble of member_id
    ! !CALLING SEQUENCE:
    ! Called by: PDAF_filter_init    (as U_ens_init)
    !EOP

    ! *** local variables ***
    INTEGER, DIMENSION(1:4) :: start  ! start of the slice we want to read from the netcdf file
    INTEGER, DIMENSION(1:4) :: ct  ! count/size of the slice we want to read from the netcdf file

    character(len = nf90_max_name) :: RecordDimName


    !TODO: Write testcase to check that dimensions are good!

    ! **********************
    ! *** INITIALIZATION ***
    ! **********************

    ! *** Generate full ensemble on filter-PE 0 ***
    IF (mype_filter==0) THEN
        WRITE (*, '(/9x, a)') 'Initialize hidden state ensemble'
        WRITE (*, '(9x, a)') '--- read ensemble from files'
        WRITE (*, '(9x, a, i5)') '--- Ensemble size:  ', dim_ens
    END IF



    ! ********************************
    ! *** Read ensemble from files ***
    ! ********************************


    if (member_id==0) THEN
        ! only open for the first member, save file handle in global variables ;)
        ! would be a bit more elegant to open all at once but traversing this information
        ! into C is shitty...

        ! TODO: here is a static filename for now... path realtive to server cwd
        call check( nf90_open("./init_ens.nc", nf90_nowrite, ncid) )

        call check(nf90_inq_dimid(ncid, "longitude",     dimid_x))
        call check(nf90_inq_dimid(ncid, "latitude",     dimid_y))
        call check(nf90_inq_dimid(ncid, "depth", dimid_z))
        call check(nf90_inq_dimid(ncid, "time", dimid_t))  ! TODO: rename this by member_id

        call check(nf90_inquire_dimension(ncid, dimid_x, recorddimname, dim_x))
        call check(nf90_inquire_dimension(ncid, dimid_y, recorddimname, dim_y))
        call check(nf90_inquire_dimension(ncid, dimid_z, recorddimname, dim_z))
        call check(nf90_inquire_dimension(ncid, dimid_t, recorddimname, dim_t))
        print *, "dim_x = ", dim_x
        print *, "dim_y = ", dim_y
        print *, "dim_z = ", dim_z
        print *, "dim_t = ", dim_t
        print *,  (dim_x*dim_y*dim_z), " == ", (50*50*12), "?"

        call check( nf90_inq_varid(ncid, "dens", dens_varid) )
        call check( nf90_inq_varid(ncid, "satur", satur_varid) )
    END IF


    ! but is x y z t
    ! is double checked and works like this with parflow. TODO a testcase would still be
    ! nice!
    print *, "member_id = ", member_id, ", mype_world = ", mype_world
    print *, "dm_p = ", dim_p
    start = (/ 1+25*mype_world, 1, 1, member_id+1 /)
    ct = (/ 25, 50, 12, 1 /)
    call check(nf90_get_var(ncid, dens_varid, hidden_state_p(1:(50*50*12/2)), &
        start, &
        ct))
    call check(nf90_get_var(ncid, satur_varid, hidden_state_p((50*50*12/2+1):(2*50*50*12/2)), &
        start, &
        ct))

    ! print *,"dens:", hidden_state_p(1), state_p(2), " ....", state_p(15000)
    ! print *,"satur:", hidden_state_p(15001), state_p(15002), " ....", state_p(30000)

    IF (member_id==dim_ens-1) THEN
    ! close after last member...
        call check( nf90_close(ncid) )
    END IF

! ****************
! *** clean up ***
! ****************


END SUBROUTINE init_ens_hidden
