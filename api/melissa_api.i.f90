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


function melissa_commit(comm) bind(c, name = 'melissa_commit')
    use ISO_C_BINDING, only: C_INT
    integer(kind=C_INT) :: melissa_commit
    integer(kind=C_INT) :: comm
end function melissa_commit


#define add_chunk_wrapper(TYPELETTER, CTYPE, FORTRANTYPE) \
__NL__ subroutine melissa_add_chunk_##TYPELETTER(values, amount, is_assimilated)& \
__NL__     bind(c, name = __s__melissa_add_chunk_##TYPELETTER##__s__) \
__NL__     use ISO_C_BINDING, only: CTYPE, C_INT \
__NL__     FORTRANTYPE(kind=CTYPE), intent(inout), dimension(*) :: values \
__NL__     integer(kind=C_INT), value, intent(in) :: amount \
__NL__     integer(kind=C_INT), value, intent(in) :: is_assimilated \
__NL__ end subroutine melissa_add_chunk_##TYPELETTER \


add_chunk_wrapper(r, C_FLOAT, real)
add_chunk_wrapper(i, C_INT, integer)
add_chunk_wrapper(d, C_DOUBLE, real)
add_chunk_wrapper(l, C_INT, integer)
add_chunk_wrapper(c, C_CHAR, character)

#undef add_chunk_wrapper

end interface


