! Fortran interface

interface


subroutine melissa_init_no_mpi(field_name,&
      local_vect_size, local_hidden_vect_size) bind(c, name = 'melissa_init_no_mpi')
    use ISO_C_BINDING, only: C_INT, C_CHAR
    character(kind=C_CHAR),intent(in),dimension(*) :: field_name
    integer(C_INT), intent(in) :: local_vect_size
    integer(C_INT), intent(in) :: local_hidden_vect_size
end subroutine melissa_init_no_mpi

subroutine melissa_init_f(field_name,&
                        local_vect_size,&
                        local_hidden_vect_size, comm) bind(c, name = 'melissa_init_f')
    use ISO_C_BINDING, only: C_INT, C_CHAR
    character(kind=C_CHAR),dimension(*) :: field_name
    integer(kind=C_INT) :: local_vect_size
    integer(kind=C_INT) :: local_hidden_vect_size
    integer(kind=C_INT) :: comm
end subroutine melissa_init_f


function melissa_expose_hidden(field_name,&
    values, hidden_values) bind(c, name = 'melissa_expose')
    use ISO_C_BINDING, only: C_CHAR, C_DOUBLE, C_INT
    integer(kind=C_INT) :: melissa_expose_hidden
    character(kind=C_CHAR), intent(in), dimension(*) :: field_name
    real(kind=C_DOUBLE), intent(inout), dimension(*) :: values
    real(kind=C_DOUBLE), intent(inout), dimension(*) :: hidden_values
end function melissa_expose_hidden

function melissa_expose(field_name,&
    values) bind(c, name = 'melissa_expose_f')
    use ISO_C_BINDING, only: C_CHAR, C_DOUBLE, C_INT
    integer(kind=C_INT) :: melissa_expose
    character(kind=C_CHAR), intent(in), dimension(*) :: field_name
    real(kind=C_DOUBLE), intent(inout), dimension(*) :: values
end function melissa_expose

end interface
