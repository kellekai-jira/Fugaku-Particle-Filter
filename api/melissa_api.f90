! Fortran interface

interface


subroutine melissa_init_no_mpi(field_name,&
      local_vect_size) bind(c, name = 'melissa_init_no_mpi')
    use ISO_C_BINDING, only: C_INT, C_CHAR
    character(kind=C_CHAR),intent(in),dimension(*) :: field_name
    integer(C_INT), intent(in) :: local_vect_size
end subroutine melissa_init_no_mpi

function melissa_expose(field_name,&
    values) bind(c, name = 'melissa_expose')
    use ISO_C_BINDING, only: C_BOOL, C_CHAR, C_DOUBLE
    logical(C_BOOL) :: melissa_expose
    character(kind=C_CHAR), intent(in), dimension(*) :: field_name
    real(kind=C_DOUBLE), intent(inout), dimension(*) :: values
end function melissa_expose

end interface
