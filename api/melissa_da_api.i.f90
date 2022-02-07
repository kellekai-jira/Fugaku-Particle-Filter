! Fortran interface

interface


subroutine melissa_init_no_mpi(field_name,&
      local_vect_size, local_hidden_vect_size) bind(c, name = 'melissa_init_no_mpi')
    use ISO_C_BINDING, only: C_CHAR, C_SIZE_T
    character(kind=C_CHAR),intent(in),dimension(*) :: field_name
    integer(C_SIZE_T), intent(in) :: local_vect_size
    integer(C_SIZE_T), intent(in) :: local_hidden_vect_size
end subroutine melissa_init_no_mpi

subroutine melissa_init_f(field_name,&
                        local_vect_size,&
                        local_hidden_vect_size, comm) bind(c, name = 'melissa_init_f')
    use ISO_C_BINDING, only: C_INT, C_INT64_T, C_CHAR, C_SIZE_T
    character(kind=C_CHAR), dimension(*) :: field_name
    integer(kind=C_INT64_T) :: local_vect_size
    integer(kind=C_INT64_T) :: local_hidden_vect_size
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

subroutine melissa_register_weight_function( func ) bind (C, name="melissa_register_weight_function")
  use, intrinsic :: iso_c_binding
	type(c_funptr), value, intent(in) :: func
end subroutine


function melissa_expose(field_name,&
    values, state_size, expose_mode) bind(c, name = 'melissa_expose_f')
    use ISO_C_BINDING, only: C_CHAR, C_DOUBLE, C_INT, C_INT64_T
    integer(kind=C_INT) :: melissa_expose
    integer(kind=C_INT64_T) :: state_size
    integer(kind=C_INT) :: expose_mode
    character(kind=C_CHAR), intent(in), dimension(*) :: field_name
    real(kind=C_DOUBLE), intent(inout), dimension(*) :: values
end function melissa_expose


function melissa_commit_chunks_f(comm_fortran) bind(c, name = 'melissa_commit_chunks_f')
    use ISO_C_BINDING, only: C_INT
    integer(kind=C_INT) :: melissa_commit_chunks_f
    integer(kind=C_INT) :: comm_fortran
end function melissa_commit_chunks_f


#define add_chunk_wrapper(TYPELETTER, CTYPE, FORTRANTYPE) \
__NL__ subroutine melissa_add_chunk_##TYPELETTER(varid, index_map, values, count, is_assimilated)& \
__NL__     bind(c, name = __s__melissa_add_chunk_##TYPELETTER##__s__) \
__NL__     use ISO_C_BINDING, only: CTYPE, C_INT, C_SIZE_T \
__NL__     integer(kind=C_INT), intent(in) :: varid \
__NL__     integer(kind=C_INT), dimension(*), intent(in) :: index_map \
__NL__     FORTRANTYPE(kind=CTYPE), intent(inout), dimension(*) :: values \
__NL__     integer(kind=C_SIZE_T), intent(in) :: count \
__NL__     integer(kind=C_INT), intent(in) :: is_assimilated \
__NL__ end subroutine melissa_add_chunk_##TYPELETTER \
__NL__ \
__NL__ subroutine melissa_add_chunk_##TYPELETTER##_d(varid, index_map, values, count, is_assimilated)& \
__NL__     bind(c, name = __s__melissa_add_chunk_##TYPELETTER##_d__s__) \
__NL__     use ISO_C_BINDING, only: CTYPE, C_INT, C_SIZE_T \
__NL__     integer(kind=C_INT), intent(in) :: varid \
__NL__     integer(kind=C_INT), dimension(*), intent(in) :: index_map \
__NL__     FORTRANTYPE(kind=CTYPE), intent(inout) :: values \
__NL__     integer(kind=C_SIZE_T), intent(in) :: count \
__NL__     integer(kind=C_INT), intent(in) :: is_assimilated \
__NL__ end subroutine melissa_add_chunk_##TYPELETTER##_d \



add_chunk_wrapper(r, C_FLOAT, real)
add_chunk_wrapper(i, C_INT, integer)
add_chunk_wrapper(d, C_DOUBLE, real)
add_chunk_wrapper(l, C_INT, LOGICAL)
add_chunk_wrapper(c, C_CHAR, character)

#undef add_chunk_wrapper

function melissa_get_current_state_id() bind(c, name = 'melissa_get_current_state_id')
    use ISO_C_BINDING, only: C_INT
    integer(kind=C_INT) :: melissa_get_current_state_id
end function melissa_get_current_state_id

function melissa_is_runner() bind(c, name='melissa_is_runner')
    use ISO_C_BINDING, only: C_INT
    integer(kind=C_INT) :: melissa_is_runner
end function melissa_is_runner


!!! returns new comm world as fortran communicator
function melissa_comm_init_f(old_comm_world_fortran) bind(c, name='melissa_comm_init_f')
    use ISO_C_BINDING, only: C_INT
    integer(kind=C_INT) melissa_comm_init_f
    integer(kind=C_INT) old_comm_world_fortran
end function melissa_comm_init_f

end interface


