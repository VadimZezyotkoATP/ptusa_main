#include <stdlib.h>
#include <vector>
#include <stdio.h>

#include "tech_def.h"

#include "lua_manager.h"

#include "errors.h"
//-----------------------------------------------------------------------------
auto_smart_ptr < tech_object_manager > tech_object_manager::instance;
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
tech_object::tech_object( const char* new_name, u_int number, u_int type,
    const char *name_Lua,
    u_int operations_count,
    u_int timers_count,
    u_int par_float_count, u_int runtime_par_float_count,
    u_int par_uint_count,
    u_int runtime_par_uint_count) :
        par_float( saved_params_float( par_float_count ) ),
        rt_par_float( run_time_params_float( runtime_par_float_count ) ),
        par_uint( saved_params_u_int_4( par_uint_count ) ),
        rt_par_uint( run_time_params_u_int_4( runtime_par_uint_count ) ),
        timers( timers_count ),
        number( number ),
        type( type ),
        cmd( 0 ),
        operations_count( operations_count ),
        modes_time( run_time_params_u_int_4( operations_count, "MODES_TIME" ) ),
        operations_manager( 0 )
    {
    u_int state_size_in_int4 = operations_count / 32; // ������ ��������� � double word.
    if ( operations_count % 32 > 0 ) state_size_in_int4++;
    for( u_int i = 0; i < state_size_in_int4; i++ )
        {
        state.push_back( 0 );
        }

    for ( u_int i = 0; i < operations_count; i++ )
        {
        available.push_back( 0 );
        }

    strncpy( name, new_name, C_MAX_NAME_LENGTH );
    strncpy( this->name_Lua, name_Lua, C_MAX_NAME_LENGTH );

    operations_manager = new operation_manager( operations_count, this );
    }
//-----------------------------------------------------------------------------
tech_object::~tech_object()
    {
    for ( u_int i = 0; i < errors.size(); i++ )
        {
        delete errors[ i ];
        errors[ i ] = 0;
        }
    }
//-----------------------------------------------------------------------------
int tech_object::init_params()
    {
    par_float.reset_to_0();
    par_uint.reset_to_0();

    return 0;
    }
//-----------------------------------------------------------------------------
int tech_object::init_runtime_params()
    {
    rt_par_float.reset_to_0();
    rt_par_uint.reset_to_0();

    return 0;
    }
//-----------------------------------------------------------------------------
int tech_object::set_mode( u_int operation_n, int newm )
    {
    int res = 0;

    static char   white_spaces[ 256 ] = "";
    static u_char idx = 0;
    if ( G_DEBUG )
        {
        printf( "%sStart \'%.40s\' [%2u] set mode = %2u --> %s.\n",
            white_spaces, name, number, operation_n,
            newm == 0 ? "OFF" : ( newm == 1 ? "ON" : ( newm == 2 ? "PAUSE" : 
            ( newm == 3 ? "STOP" : ( newm == 4 ? "WAIT" : "?" ) ) ) ) );

        white_spaces[ idx++ ] = ' ';
        white_spaces[ idx++ ] = ' ';
        white_spaces[ idx++ ] = ' ';
        white_spaces[ idx++ ] = 0;
        }

    if ( operation_n > operations_count )
        {
        res = 3;
        }
    if ( 0 == operation_n )
        {
        res = 4;
        }

    int i = operation_n - 1;
    if ( 0 == res )    
        {
        switch ( newm )
            {
            case operation::PAUSE:                
                state[ i / 32 ] = state[ i / 32 ] | 1UL << i % 32;
                ( *operations_manager )[ operation_n ]->pause();
                lua_on_pause( operation_n );
                break;

            case operation::STOP:                
                state[ i / 32 ] = state[ i / 32 ] | 1UL << i % 32;
                ( *operations_manager )[ operation_n ]->stop();
                lua_on_stop( operation_n );
                break;
            
            default:
                if ( newm != 0 ) newm = 1;
                operation* op = ( *operations_manager )[ operation_n ];

                if ( get_mode( operation_n ) == newm ) 
                    {
                    if ( newm == 1 && op->get_state() != operation::RUN )                     
                        {
                        op->start();
                        lua_on_start( operation_n );
                        }
                    else                    
                        {
                        res = 1;
                        }
                    }
                else
                    {
                    if ( newm == 0 ) // Off mode.
                        {

                        if ( ( res = lua_check_off_mode( operation_n ) ) == 0 ) // Check if possible.
                            {
                            int idx = operation_n - 1;
                            state[ idx / 32 ] = state[ idx / 32 ] & ~( 1UL << idx % 32 );

                            op->final();
                            lua_final_mode( operation_n );
                            }
                        else
                            {
                            res += 100;
                            }
                        }
                    else
                        {
                        //�������� ������ �� �������� �� ���������.
                        const int ERR_STR_SIZE = 41;
                        char res_str[ ERR_STR_SIZE ] = "���. ����� ";

                        int len = strlen( res_str );
                        res = op->check_devices_on_run_state(
                            res_str + len, ERR_STR_SIZE - len );
                        if ( res && is_check_mode( operation_n ) == 1 )
                            {
                            set_err_msg( res_str, operation_n );
                            res = operation_n + 1000;
                            }
                        else
                            {
                            bool is_dev_err = res ? true : false;
                            //�������� ������ �� �������� �� ���������.

                            if ( ( res = lua_check_on_mode( operation_n ) ) == 0 ) // Check if possible.
                                {
                                op->start();
                                lua_init_mode( operation_n );
                                lua_on_start( operation_n );

                                int idx = operation_n - 1;
                                state[ idx / 32 ] = state[ idx / 32 ] | 1UL << idx % 32;                                
                                
                                //�������� ������ �� �������� �� ���������.
                                if ( is_dev_err )
                                    {
                                    set_err_msg( res_str, operation_n, 0, ERR_ON_WITH_ERRORS );
                                    }
                                //�������� ������ �� �������� �� ���������.
                                }
                            else
                                {
                                res += 100;
                                }
                            }
                        }
                    }
                break;
            }
        }

    if ( G_DEBUG )
        {
        idx -= 4;
        white_spaces[ idx ] = 0;

        printf( "%sEnd \'%.40s\' [%2u] set mode = %2u --> %s, res = %d",
            white_spaces, name, number, operation_n,
            newm == 0 ? "OFF" : ( newm == 1 ? "ON" : ( newm == 2 ? "PAUSE" : 
            ( newm == 3 ? "STOP" : ( newm == 4 ? "WAIT" : "?" ) ) ) ), res );

        switch ( res )
            {
            case 1:
                printf( " (is already %s).\n", newm == 0 ? "OFF" : " ON" );
                break;

            case 3:
                printf( " (mode %d > modes count %d).\n",  operation_n, operations_count );
                break;

            case 4:
                printf( " (no zero (0) mode).\n" );
                break;

             default:
                 if ( res > 100 )
                    {
                    printf( " (can't on).\n" );
                    break;
                    }

                printf( ".\n" );
                break;
            }

        for ( u_int i = 0; i < state.size(); i++ )
            {
            printf( "%sstate[ %d ] = %u (",
                white_spaces, i, state[ i ] );
            print_binary( state[ i ] );
            printf( ")\n" );
            }
        printf( "\n" );
        }

    return res;
    }
//-----------------------------------------------------------------------------
int tech_object::get_mode( u_int operation )
    {
    if ( operation > operations_count || 0 == operation ) return 0;

    int idx = operation - 1;
    return ( int )( ( u_int_4 ) ( state.at( idx / 32 ) >> idx % 32 ) & 1 );
    }
//-----------------------------------------------------------------------------
int tech_object::get_operation_state( u_int operation )
    {
    if ( operation > operations_count || 0 == operation ) return 0;

    return ( *operations_manager )[ operation ]->get_state();
    }
//-----------------------------------------------------------------------------
int tech_object::check_on_mode( u_int operation, char* reason )
    {
    if ( operation > operations_count || 0 == operation ) return 0;

    return (*operations_manager)[ operation ]->check_on_run_state( reason );
    }
//-----------------------------------------------------------------------------
int tech_object::evaluate()
    {
    for ( u_int i = 0; i < operations_count; i++ )
        {
        int idx = i + 1;
        operation* op = ( *operations_manager )[ idx ];

        modes_time[ idx ] = op->evaluation_time() / 1000;

        if ( get_mode( idx ) == 1 )
        	{
            op->evaluate();
        	}

        if ( G_PAC_INFO()->par[ PAC_info::P_AUTO_PAUSE_OPER_ON_DEV_ERR ] == 0 && 
            op->get_state() == operation::RUN )
        	{
            //�������� ������ �� �������� �� ���������.
            const int ERR_STR_SIZE = 41;
            char res_str[ ERR_STR_SIZE ] = "������ ��������� ";

            int len = strlen( res_str );
            int res = op->check_devices_on_run_state( res_str + len,
                ERR_STR_SIZE - len );
            if ( res && is_check_mode( idx ) == 1 )
                {
                set_err_msg( res_str, idx, 0, ERR_TO_FAIL_STATE );
                op->pause();
                lua_on_pause( idx );
                }
        	}
        
        }
    return 0;
    }
//-----------------------------------------------------------------------------
int tech_object::check_off_mode( u_int mode )
    {
    return 0;
    }
//-----------------------------------------------------------------------------
int tech_object::final_mode( u_int mode )
    {
    if ( mode > operations_count || 0 == mode ) return 1;
        
    operations_manager->reset_idle_time();
        
    return 0;
    }
//-----------------------------------------------------------------------------
int tech_object::lua_exec_cmd( u_int cmd )
    {
    tech_object::exec_cmd( cmd );

    return lua_manager::get_instance()->int_exec_lua_method( name_Lua, "exec_cmd",
        cmd, "int tech_object::lua_exec_cmd( u_int cmd )" );
    }
//-----------------------------------------------------------------------------
int tech_object::lua_check_on_mode( u_int mode )
    {
    char err_msg[ 200 ] = "";

    if ( G_PAC_INFO()->par[ PAC_info::P_RESTRICTIONS_MODE ] == 0 )
        {
        //�������� �� ������� ��� ���������� ��������, ����������� �������.
        //1. �������� �������� �������.
        lua_State *L = lua_manager::get_instance()->get_Lua();
        lua_getfield( L, LUA_GLOBALSINDEX, "restrictions" );
        if ( lua_istable( L, -1 ) )
            {
            lua_rawgeti( L, -1, serial_idx );           //����� �������.
            if ( lua_istable( L, -1 ) )
                {
                lua_rawgeti( L, -1, mode );             //����� ��������.
                if ( lua_istable( L, -1 ) )
                    {
                    //1 ����������� ������ �������.
                    lua_rawgeti( L, -1, 1 );
                    if ( lua_istable( L, -1 ) )
                        {
                        for ( u_int i = 1; i <= get_modes_count(); i++ )
                            {
                            lua_rawgeti( L, -1, i );    //����� �����������.
                            if ( lua_isnumber( L, -1 ) )
                                {
                                //���� �����������, ��������� �� ���������� ��������.
                                if ( get_mode( i ) )
                                    {
                                    set_err_msg( err_msg, mode, i, ERR_CANT_ON_2_OPER );
                                    lua_pop( L, 5 );
                                    return 1000 + i;
                                    }
                                }
                            lua_pop( L, 1 );
                            }
                        }
                    lua_pop( L, 1 );

                    //2 ����������� �� ������ ��������.
                    lua_rawgeti( L, -1, 2 );
                    if ( lua_istable( L, -1 ) )
                        {
                        for ( u_int k = 1; k <= G_TECH_OBJECT_MNGR()->get_count(); k++ )
                            {
                            lua_rawgeti( L, -1, k ); //������ � ������� k.
                            if ( lua_istable( L, -1 ) )
                                {
                                tech_object* t_obj = G_TECH_OBJECT_MNGR()->get_tech_objects( k - 1 );
                                for ( u_int i = 1;
                                    i <= t_obj->get_modes_count(); i++ )
                                    {
                                    lua_rawgeti( L, -1, i );    //����� �����������.
                                    if ( lua_isnumber( L, -1 ) )
                                        {
                                        //���� �����������, ��������� �� ���������� ��������.
                                        if ( t_obj->get_mode( i ) )
                                            {
                                            snprintf( err_msg, sizeof( err_msg ),
                                                "��� �������� �������� %.1d \'%.40s\' ��� ������� \'%.40s %d\'",
                                                i, 
                                                (*t_obj->get_modes_manager())[i]->get_name(),
                                                t_obj->get_name(), t_obj->get_number() );

                                            set_err_msg( err_msg, mode, i, ERR_CANT_ON_2_OBJ );
                                            lua_pop( L, 6 );
                                            return 10000 + i;
                                            }
                                        }
                                    lua_pop( L, 1 );
                                    }
                                }
                            lua_pop( L, 1 );
                            }
                        }
                    lua_pop( L, 1 );
                    }
                lua_pop( L, 1 );
                }
            lua_pop( L, 1 );
            }
        lua_pop( L, 1 );
        }

    if ( int res = tech_object::check_on_mode( mode, err_msg ) )
        {
        set_err_msg( err_msg, mode );

        return 1000 + res;
        }

    //TODO. ���������� �������� �������. ��������� ��� �������������.
    //�������� �� ������� ������� check_on_mode.
    lua_getfield( lua_manager::get_instance()->get_Lua(), LUA_GLOBALSINDEX,
        name_Lua );
    lua_getfield( lua_manager::get_instance()->get_Lua(), -1, "check_on_mode" );
    lua_remove( lua_manager::get_instance()->get_Lua(), -2 );

    if ( lua_isfunction( lua_manager::get_instance()->get_Lua(), -1 ) )
        {
        return lua_manager::get_instance()->int_exec_lua_method( name_Lua,
            "check_on_mode", mode,
            "int tech_object::lua_check_on_mode( u_int mode )" );
        }

    //�������� �� ������� ������� user_check_operation_on.
    lua_getfield( lua_manager::get_instance()->get_Lua(), LUA_GLOBALSINDEX,
        name_Lua );
    lua_getfield( lua_manager::get_instance()->get_Lua(), -1, "user_check_operation_on" );
    lua_remove( lua_manager::get_instance()->get_Lua(), -2 );

    if ( lua_isfunction( lua_manager::get_instance()->get_Lua(), -1 ) )
        {
        return lua_manager::get_instance()->int_exec_lua_method( name_Lua,
            "user_check_operation_on", mode,
            "int tech_object::lua_check_on_mode( u_int mode )" );
        }
    

    return 0;
    }
//-----------------------------------------------------------------------------
void tech_object::lua_init_mode( u_int mode )
    {
    lua_manager::get_instance()->int_exec_lua_method( name_Lua,
        "init_mode", mode, "void tech_object::lua_init_mode( u_int mode )" );

    //��������� ��������� �����������.
    for ( u_int i = 1; i <= get_modes_count(); i++ )
        {
        available[ i - 1 ] = 0;
        }
    for ( u_int i = 1; i <= get_modes_count(); i++ )
        {
        if ( get_mode( i ) == 1 )
            {
            check_availability( i );
            }
        }
    check_availability( mode );
    }
//-----------------------------------------------------------------------------
void tech_object::check_availability( u_int operation_n )
    {
    //������ �� ��������� ������ ��������, �������������� �������.
    //1. �������� �������� �������.
    lua_State *L = lua_manager::get_instance()->get_Lua();
    lua_getfield( L, LUA_GLOBALSINDEX, "restrictions" );
    if ( lua_istable( L, -1 ) )
        {
        lua_rawgeti( L, -1, serial_idx );           //����� �������.
        if ( lua_istable( L, -1 ) )
            {
            //������ �� ���� ���������.
            for ( u_int i = 1; i <= get_modes_count(); i++ )
                {
                if ( i == operation_n ) continue;

                lua_rawgeti( L, -1, i );            //����� ��������.
                if ( lua_istable( L, -1 ) )
                    {
                    //1 ����������� ������ �������.
                    lua_rawgeti( L, -1, 1 );
                    if ( lua_istable( L, -1 ) )
                        {
                        lua_rawgeti( L, -1, operation_n );
                        if ( lua_isnumber( L, -1 ) )
                            {
                            available[ i - 1 ]++;
                            }
                        lua_pop( L, 1 );
                        }
                    lua_pop( L, 1 );
                    }
                lua_pop( L, 1 );
                }
            }
        lua_pop( L, 1 );
        }
    lua_pop( L, 1 );
    }
//-----------------------------------------------------------------------------
int tech_object::lua_check_off_mode( u_int mode )
    {
    tech_object::check_off_mode( mode );

    return lua_manager::get_instance()->int_exec_lua_method( name_Lua,
        "check_off_mode", mode, "int tech_object::lua_check_off_mode( u_int mode )" );
    }
//-----------------------------------------------------------------------------
int tech_object::lua_final_mode( u_int mode )
    {
    tech_object::final_mode( mode );

    lua_manager::get_instance()->int_exec_lua_method( name_Lua,
        "final_mode", mode, "int tech_object::lua_final_mode( u_int mode )" );

    //������������ ������ �� ��������� ������ ��������, ��������������
    //�������, � ������ �� ����������� ��������.
    //1. �������� �������� �������.
    lua_State *L = lua_manager::get_instance()->get_Lua();
    lua_getfield( L, LUA_GLOBALSINDEX, "restrictions" );
    if ( lua_istable( L, -1 ) )
        {
        lua_rawgeti( L, -1, serial_idx );           //����� �������.
        if ( lua_istable( L, -1 ) )
            {
            //������ �� ���� ���������.
            for ( u_int i = 1; i <= get_modes_count(); i++ )
                {
                if ( i == mode ) continue;

                lua_rawgeti( L, -1, i );            //����� ��������.
                if ( lua_istable( L, -1 ) )
                    {
                    //1 ����������� ������ �������.
                    lua_rawgeti( L, -1, 1 );
                    if ( lua_istable( L, -1 ) )
                        {
                        lua_rawgeti( L, -1, mode );
                        if ( lua_isnumber( L, -1 ) )
                            {
                            available[ i - 1 ]--;
                            }
                        lua_pop( L, 1 );
                        }
                    lua_pop( L, 1 );
                    }
                lua_pop( L, 1 );
                }

            lua_rawgeti( L, -1, mode );            //������� ��������.
            if ( lua_istable( L, -1 ) )
                {
                //1 ����������� �� ����������� �������� ������ �������.
                lua_rawgeti( L, -1, 3 );
                if ( lua_istable( L, -1 ) )
                    {
                    //������ �� ���� ���������.
                    for ( u_int i = 1; i <= get_modes_count(); i++ )
                        {
                        lua_rawgeti( L, -1, i );
                        if ( lua_isnumber( L, -1 ) )
                            {
                            available[ i - 1 ]++;
                            }
                        lua_pop( L, 1 );
                        }
                    }
                lua_pop( L, 1 );
                }
            lua_pop( L, 1 );
            }  

        lua_pop( L, 1 ); 
        }
    lua_pop( L, 1 );

    return 0;
    }
//-----------------------------------------------------------------------------
int tech_object::lua_init_params()
    {
    tech_object::init_params();

    //�������� �� ������� ������� ������������� ���������� uint.
    lua_getfield( lua_manager::get_instance()->get_Lua(), LUA_GLOBALSINDEX,
        name_Lua );
    lua_getfield( lua_manager::get_instance()->get_Lua(), -1, "init_params_uint" );
    lua_remove( lua_manager::get_instance()->get_Lua(), -2 );

    if ( lua_isfunction( lua_manager::get_instance()->get_Lua(), -1 ) )
        {
        lua_manager::get_instance()->int_exec_lua_method( name_Lua,
            "init_params_uint", 0, "int tech_object::lua_init_params()" );
        }
    //������� "init_params_uint" �� �����.
    lua_remove( lua_manager::get_instance()->get_Lua(), -1 );

    //�������� �� ������� ������� ������������� ���������� float.
    lua_getfield( lua_manager::get_instance()->get_Lua(), LUA_GLOBALSINDEX,
        name_Lua );
    lua_getfield( lua_manager::get_instance()->get_Lua(), -1, "init_params_float" );
    lua_remove( lua_manager::get_instance()->get_Lua(), -2 );

    if ( lua_isfunction( lua_manager::get_instance()->get_Lua(), -1 ) )
        {
        lua_manager::get_instance()->int_exec_lua_method( name_Lua,
            "init_params_float", 0, "int tech_object::lua_init_params()" );
        }
    //������� "init_params_float" �� �����.
    lua_remove( lua_manager::get_instance()->get_Lua(), -1 );

    //�������� �� ������� ������� ������������� ������� ���������� uint.
    lua_getfield( lua_manager::get_instance()->get_Lua(), LUA_GLOBALSINDEX,
        name_Lua );
    lua_getfield( lua_manager::get_instance()->get_Lua(), -1,
        "init_rt_params_uint" );
    lua_remove( lua_manager::get_instance()->get_Lua(), -2 );

    if ( lua_isfunction( lua_manager::get_instance()->get_Lua(), -1 ) )
        {
        lua_manager::get_instance()->int_exec_lua_method( name_Lua,
            "init_rt_params_uint", 0, "int tech_object::lua_init_params()" );
        }
    //������� "init_params_uint" �� �����.
    lua_remove( lua_manager::get_instance()->get_Lua(), -1 );

    //�������� �� ������� ������� ������������� ������� ���������� float.
    lua_getfield( lua_manager::get_instance()->get_Lua(), LUA_GLOBALSINDEX,
        name_Lua );
    lua_getfield( lua_manager::get_instance()->get_Lua(), -1,
        "init_rt_params_float" );
    lua_remove( lua_manager::get_instance()->get_Lua(), -2 );

    if ( lua_isfunction( lua_manager::get_instance()->get_Lua(), -1 ) )
        {
        lua_manager::get_instance()->int_exec_lua_method( name_Lua,
            "init_rt_params_float", 0, "int tech_object::lua_init_params()" );
        }
    //������� "init_params_float" �� �����.
    lua_remove( lua_manager::get_instance()->get_Lua(), -1 );

    return 0;
    }
//-----------------------------------------------------------------------------
int tech_object::lua_init_runtime_params()
    {
    tech_object::init_runtime_params();

    return lua_manager::get_instance()->int_exec_lua_method( name_Lua,
        "init_runtime_params", 0, "int tech_object::lua_init_runtime_params()" );
    }
//-----------------------------------------------------------------------------
int  tech_object::lua_on_pause( u_int mode )
    {
    lua_manager::get_instance()->int_exec_lua_method( name_Lua,
        "on_pause", mode, "int tech_object::lua_on_pause( u_int mode )" );
    return 0;
    }
//-----------------------------------------------------------------------------
int  tech_object::lua_on_stop( u_int mode )
    {
    lua_manager::get_instance()->int_exec_lua_method( name_Lua,
        "on_stop", mode, "int tech_object::lua_on_stop( u_int mode )" );
    return 0;
    }
//-----------------------------------------------------------------------------
int  tech_object::lua_on_start( u_int mode )
    {
    lua_manager::get_instance()->int_exec_lua_method( name_Lua,
        "on_start", mode, "int tech_object::lua_on_start( u_int mode )" );
    return 0;
    }
//-----------------------------------------------------------------------------
int tech_object::save_device( char *buff )
    {
    int res = 
        sprintf( buff, "t.%s = t.%s or {}\nt.%s=\n\t{\n", name_Lua, name_Lua,
        name_Lua );
    
    //��������� � �������.
    res += sprintf( buff + res, "\tCMD=%lu,\n", ( u_long ) cmd );    
    res += sprintf( buff + res, "\tST=\n\t\t{\n\t\t" );
    
    for ( u_int i = 0; i < state.size(); i++ )
        {
        res += sprintf( buff + res, "%lu, ", ( long int ) state[ i ] );        
        }
    res += sprintf( buff + res, "\n\t\t},\n" );
    
    //��������.
    res += sprintf( buff + res, "\tMODES=\n\t\t{\n\t\t" );    
    for ( u_int i = 1; i <= operations_count; i++ )
        {
        res += sprintf( buff + res, "%d, ", get_operation_state( i ) ? 1 : 0 );
        }
    res += sprintf( buff + res, "\n\t\t},\n" );
    res += sprintf( buff + res, "\tOPERATIONS=\n\t\t{\n\t\t" );
    for ( u_int i = 1; i <= operations_count; i++ )
        {
        res += sprintf( buff + res, "%d, ", get_operation_state( i ) );
        }
    res += sprintf( buff + res, "\n\t\t},\n" );

    //����������� ��������.
    res += sprintf( buff + res, "\tAVAILABILITY=\n\t\t{\n\t\t" );      
    for ( u_int i = 1; i <= operations_count; i++ )
        {
        res += sprintf( buff + res, "%d, ", available.at( i - 1 ) == 0 ? 1 : 0 );
        }
    res += sprintf( buff + res, "\n\t\t},\n" );
    
    //����� �������.
    static char up_time_str [ 50 ] = { 0 };
    u_int_4 up_hours;
    u_int_4 up_mins;
    u_int_4 up_secs;

    up_secs = operations_manager->get_idle_time() / 1000;

    up_hours = up_secs / ( 60 * 60 );
    up_mins = up_secs / 60 % 60 ;
    up_secs %= 60;

    sprintf( up_time_str, "\tIDLE_TIME = \'%02lu:%02lu:%02lu\',\n",
        ( u_long ) up_hours, ( u_long ) up_mins, ( u_long ) up_secs );
    res += sprintf( buff + res, "%s", up_time_str );
    
    //����� �������.
    res += sprintf( buff + res, "\tMODES_TIME=\n\t\t{\n\t\t" );

    for ( u_int i = 1; i <= modes_time.get_count(); i++ )
        {
        up_secs = modes_time[ i ];

        up_hours = up_secs / ( 60 * 60 );
        up_mins = up_secs / 60 % 60 ;
        up_secs %= 60;

        if ( up_hours )
            {
            sprintf( up_time_str, "\'%02lu:%02lu:%02lu\', ",
                ( u_long ) up_hours, ( u_long ) up_mins, ( u_long ) up_secs );
            }
        else
            {
            if ( up_mins )
                {
                sprintf( up_time_str, "\'   %02lu:%02lu\', ",
                    ( u_long ) up_mins, ( u_long ) up_secs );
                }
            else
                {
                    sprintf( up_time_str, "\'      %02lu\', ",
                        (u_long)up_secs );
                    }
                    }

        res += sprintf( buff + res, "%s", up_time_str );        
        }
    res += sprintf( buff + res, "\n\t\t},\n" );
   
    //����.
    res += sprintf( buff + res, "\tMODES_STEPS=\n\t\t{\n\t\t" );  
    for ( u_int i = 0; i < operations_count; i++ )
        {
        res += sprintf( buff + res, "%d, ", 
            (*operations_manager)[ i + 1 ]->active_step());        
        }
    res += sprintf( buff + res, "\n\t\t},\n" );

    res += sprintf( buff + res, "\tMODES_RUN_STEPS=\n\t\t{\n\t\t" );
    for ( u_int i = 0; i < operations_count; i++ )
        {
        res += sprintf( buff + res, "%d, ",
            ( *operations_manager )[ i + 1 ]->get_run_step() );
        }
    res += sprintf( buff + res, "\n\t\t},\n" );

    
    for ( u_int i = 1; i <= operations_count; i++ )
        {        
        u_int steps_count = ( *operations_manager )[ i ]->get_run_steps_count();
        if ( steps_count == 0 )
            {
            continue;
            }

        res += sprintf( buff + res, "\tSTEPS%d=\n\t\t{\n\t\t", i );
        u_int run_step = ( *operations_manager )[ i ]->get_run_active_step();

        for ( u_int j = 1; j <= steps_count; j++ )
            {
            if ( run_step == j )
                {
                res += sprintf( buff + res, "1, " );
                }
            else
                {
                res += sprintf( buff + res, "0, " );
                }
            }
        res += sprintf( buff + res, "\n\t\t},\n" );
        }    

    //���������.
    res += par_float.save_device( buff + res, "\t" );
    res += par_uint.save_device( buff + res, "\t" );
    res += rt_par_float.save_device( buff + res, "\t" );
    res += rt_par_uint.save_device( buff + res, "\t" );


    res += sprintf( buff + res, "\t}\n" );   
    return res;
    }
//-----------------------------------------------------------------------------
int tech_object::set_cmd( const char *prop, u_int idx, double val )
    {
    if ( strcmp( prop, "CMD" ) == 0 )
        {
        if ( G_DEBUG )
            {
            printf( "tech_object::set_cmd() - prop = \"%s\", val = %f\n",
                prop, val );
            }

        if ( 0. == val )
            {
            cmd = 0;
            return 0;
            }

        u_int mode     = ( int ) val;
        char new_state = 0;

        if ( mode >= 1000 && mode < 2000 )      // On mode.
            {
            mode = mode - 1000;
            new_state = 1;
            }
        else
            {
            if ( mode >= 2000 && mode < 3000 )  // Off mode.
                {
                mode = mode - 2000;
                new_state = 0;
                }
            else
                {
                if ( mode >= 100000 && mode < 200000 )      // On state mode.
                    {                    
                    new_state = mode % 100;
                    mode = mode / 100 - 1000;  
                    cmd = ( int ) val;
                    return set_mode( mode, new_state );
                    }
                else
                    {                   
                    if ( G_DEBUG )
                        {
                        printf( "Error complex_state::parse_cmd - new_mode = %lu\n",
                            ( unsigned long int ) mode );
                        }
                    return 1;
                    }
                }
            }

        if ( mode > get_modes_count() + 1 )
            {
            // Command.
            cmd = lua_exec_cmd( mode );
            }
        else
            {
            // On/off mode.
            int res = set_mode( mode, new_state );
            if ( 0 == res )
                {
                cmd = ( int ) val;  // Ok.
                }
            else
                {
                cmd = res;          // ������.
                }
            }

        return 0;
        }

    if ( strcmp( prop, "S_PAR_F" ) == 0 )
        {
        if ( G_DEBUG )
            {
            printf( "\"%s%d\" %s.S_PAR_F[%d]=%f\n",
                name, number, name_Lua, idx, val );
            }

        par_float.save( idx, ( float ) val );
        return 0;
        }

    if ( strcmp( prop, "RT_PAR_F" ) == 0 )
        {
        rt_par_float[ idx ] = ( float ) val;
        return 0;
        }

    if ( strcmp( prop, "S_PAR_UI" ) == 0 )
        {
        par_uint.save( idx, ( u_int_4 ) val );
        return 0;
        }

    if ( strcmp( prop, "RT_PAR_UI" ) == 0 )
        {
        rt_par_uint[ idx ] = ( u_int_4 ) val;
        return 0;
        }

    if ( strcmp( prop, "MODES" ) == 0 )
        {
        set_mode( idx, ( int ) val );
        return 0;
        }

    if ( G_DEBUG )
        {
        printf( "Eror tech_object::set_cmd(...), prop = \"%s\", idx = %u, val = %f\n",
            prop, idx, val );
        }

    return 1;
    }
//-----------------------------------------------------------------------------
int tech_object::save_params_as_Lua_str( char* str )
    {
    int res = sprintf( str, "params{ object = \'%s\', param_name = \'%s\', "
        "par_id = %d,\n",
        name_Lua, "par_float", ID_PAR_FLOAT );
    res += par_float.save_device_ex( str + res, "", "values"  ) - 2;
    res += sprintf( str + res, "%s", " }\n" );

    res += sprintf( str + res, "params{ object = \'%s\', param_name = \'%s\', "
        "par_id = %d,\n",
        name_Lua, "rt_par_float", ID_RT_PAR_FLOAT );
    res += rt_par_float.save_device_ex( str + res, "", "values"  ) - 2;
    res += sprintf( str + res, "%s", " }\n" );

    res += sprintf( str + res, "params{ object = \'%s\', param_name = \'%s\', "
        "par_id = %d,\n",
        name_Lua, "par_uint", ID_PAR_UINT );
    res += par_uint.save_device_ex( str + res, "", "values"  ) - 2;
    res += sprintf( str + res, "%s", " }\n" );

    res += sprintf( str + res, "params{ object = \'%s\', param_name = \'%s\', "
        "par_id = %d,\n",
        name_Lua, "rt_par_uint", ID_RT_PAR_UINT );
    res += rt_par_uint.save_device_ex( str + res, "", "values"  ) - 2;
    res += sprintf( str + res, "%s", " }\n" );

    return res;
    }
//-----------------------------------------------------------------------------
int tech_object::set_param( int par_id, int index, double value )
    {
    switch ( par_id )
        {
    case ID_PAR_FLOAT:
        par_float.save( index, ( float ) value );
        break;

    case ID_RT_PAR_FLOAT:
        rt_par_float[ index ] = ( float ) value;
        break;

    case ID_PAR_UINT:
        par_uint.save( index, ( u_int ) value );
        break;

    case ID_RT_PAR_UINT:
        rt_par_uint[ index ] = ( u_int ) value;
        break;
        }

    return 0;
    }
//-----------------------------------------------------------------------------
int tech_object::is_check_mode( int mode ) const
    {
    int res = lua_manager::get_instance()->int_exec_lua_method( name_Lua,
        "is_check_mode", mode, "int tech_object::is_check_mode( u_int mode )" );

    if ( res >= 0 )
        {
        return res;
        }

    return 1;
    }
//-----------------------------------------------------------------------------
int tech_object::set_err_msg( const char *err_msg, int mode, int new_mode,
                             ERR_MSG_TYPES type /*= ERR_CANT_ON */ )
    {
    err_info *new_err = new err_info;
    static int error_number = 0;

    error_number++;
    new_err->n = error_number;
    new_err->type = type;

    switch ( type )
        {
        case ERR_CANT_ON:
            snprintf( new_err->msg, sizeof( new_err->msg ),
                "\'%.40s %d\' - �� �������� �������� %.1d \'%.40s\' - %.60s.",
                name, number, mode, ( *operations_manager )[ mode ]->get_name(), 
                err_msg );
            break;

        case ERR_ON_WITH_ERRORS:
            snprintf( new_err->msg, sizeof( new_err->msg ),
                "\'%.40s %d\' - �������� � ������� �������� %.1d \'%.40s\' - %.50s.",
                name, number, mode, ( *operations_manager )[ mode ]->get_name(), 
                err_msg );
            break;

        case ERR_OFF:
            snprintf( new_err->msg, sizeof( new_err->msg ),
                "\'%.40s %d\' - ��������� �������� %.1d \'%.40s\' - %.50s.",
                name, number, mode, ( *operations_manager )[ mode ]->get_name(), 
                err_msg );
            break;

        case ERR_OFF_AND_ON:
            snprintf( new_err->msg, sizeof( new_err->msg ),
                "\'%.40s %d\' - ������� �� %.1d \'%.40s\' � %.1d \'%.40s\'.",
                name, number, mode, ( *operations_manager )[ mode ]->get_name(),
                new_mode, ( *operations_manager )[ new_mode ]->get_name() );

            if ( strcmp( err_msg, "" ) != 0 )
                {
                snprintf( new_err->msg + strlen( new_err->msg ) - 1,
                    sizeof( new_err->msg ) - strlen( new_err->msg ) - 1,
                    " - %.50s.", err_msg );
                }
            break;

        case ERR_DURING_WORK:
            if ( mode > 0 )
                {
                snprintf( new_err->msg, sizeof( new_err->msg ),
                    "\'%.40s %d\' - �������� %.1d \'%.40s\' - %.50s.",
                    name, number, mode, ( *operations_manager )[ mode ]->get_name(), 
                    err_msg );
                }
            else
                {
                snprintf( new_err->msg, sizeof( new_err->msg ),
                    "\'%.40s %d\' - %.50s.",
                    name, number, err_msg );
                }

            break;

        case ERR_ALARM:
            snprintf( new_err->msg, sizeof( new_err->msg ),
                "\'%.40s %d\' - %.60s.", name, number, err_msg );
            break;
            
        case ERR_TO_FAIL_STATE:
            snprintf( new_err->msg, sizeof( new_err->msg ),
                "\'%.40s %d\' - ������ �������� %.1d \'%.40s\' - %.50s.",
                name, number, mode, ( *operations_manager )[ mode ]->get_name(), 
                err_msg );
            break;

        case ERR_CANT_ON_2_OPER:
            snprintf( new_err->msg, sizeof( new_err->msg ),
                "\'%.40s %d\' - �� �������� �������� %.1d \'%.40s\' - "
                "��� �������� �������� %.1d \'%.40s\'.",
                name, number, 
                mode, ( *operations_manager )[ mode ]->get_name(),
                new_mode, ( *operations_manager )[ new_mode ]->get_name() );

            if ( strcmp( err_msg, "" ) != 0 )
                {
                snprintf( new_err->msg + strlen( new_err->msg ) - 1,
                    sizeof( new_err->msg ) - strlen( new_err->msg ) - 1,
                    " - %.50s.", err_msg );
                }
            break;

        case ERR_CANT_ON_2_OBJ:
            snprintf( new_err->msg, sizeof( new_err->msg ),
                "\'%.40s %d\' - �� �������� �������� %.1d \'%.40s\' - %s.",
                name, number,
                mode, ( *operations_manager )[ mode ]->get_name(),
                err_msg );
            break;

        default:
            if ( G_DEBUG )
                {
                printf( "Error tech_object::set_err_msg(...) - unknown error type!\n" );
                debug_break;
                }
            snprintf( new_err->msg, sizeof( new_err->msg ),
                "\'%.40s\' - ����� %.1d \'%.40s\' - %.50s.",
                name, mode, ( *operations_manager )[ mode ]->get_name(), err_msg );
            break;
        }
    
    if ( G_DEBUG )
        {
        printf( "������� -> %s\n", new_err->msg );
        }

    if ( errors.size() < E_MAX_ERRORS_SIZE )
        {
        errors.push_back( new_err );
        }
    else
        {
        if ( G_DEBUG )
            {
            printf( "Error: max errors count (%d) is reached.",
                E_MAX_ERRORS_SIZE );
            }
        }

    return 0;
    }
//-----------------------------------------------------------------------------
bool tech_object::is_idle() const
    {
    for ( u_int i = 0; i < state.size(); i++ )
        {
        if ( state[ i ] > 0 )
            {
            return false;
            }
        }

    return true;
    }
//-----------------------------------------------------------------------------
bool tech_object::is_any_important_mode()
    {
    static char has_Lua_impl = 0;
    if ( has_Lua_impl == 0 )
        {
        //�������� �� ������� Lua-������� is_any_important_mode().
        lua_getfield( lua_manager::get_instance()->get_Lua(), LUA_GLOBALSINDEX,
            name_Lua );
        lua_getfield( lua_manager::get_instance()->get_Lua(), -1, 
            "is_any_important_mode" );
        lua_remove( lua_manager::get_instance()->get_Lua(), -2 );
  
        if ( lua_isfunction( lua_manager::get_instance()->get_Lua(), -1 ) )
            {
            has_Lua_impl = 2;
            }
        else
            {
            has_Lua_impl = 1;
            }
        //������� "is_any_important_mode" �� �����.
        lua_remove( lua_manager::get_instance()->get_Lua(), -1 );        
        }

    if ( has_Lua_impl == 2 )
        {
        int res = lua_manager::get_instance()->int_no_param_exec_lua_method( name_Lua,
            "is_any_important_mode", 
            "void tech_object::is_any_important_mode()" );

        return res > 0;        
        }

    for ( u_int i = 0; i < state.size(); i++ )
        {
        if ( state[ i ] > 0 )
            {
            return true;
            }
        }

    return false;
    }
//-----------------------------------------------------------------------------
int tech_object::check_operation_on( u_int operation_n )
    {
    operation* op = ( *operations_manager )[ operation_n ];

    //�������� ������ �� �������� �� ���������.
    const int S_SIZE = 41;
    char res_str[ S_SIZE ] = "���. ����� ";

    int l = strlen( res_str );
    int res = op->check_devices_on_run_state( res_str + l, S_SIZE - l );
    if ( res && is_check_mode( operation_n ) == 1 )
        {
        set_err_msg( res_str, operation_n );
        res = operation_n + 1000;
        }
    else
        {
        if ( ( res = lua_check_on_mode( operation_n ) ) == 0 ) // Check if possible.
            {
            }
        else
            {
            res += 100;
            }
        }

    return res;
    }
//-----------------------------------------------------------------------------
void tech_object::set_serial_idx( u_int idx )
    {
    serial_idx = idx;
    }
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int tech_object_manager::init_params()
    {
    for( u_int i = 0; i < tech_objects.size(); i++ )
        {
        tech_objects[ i ]->lua_init_params();
        }
    
    lua_getfield( lua_manager::get_instance()->get_Lua(), LUA_GLOBALSINDEX,
        "init_params" );

    if ( lua_isfunction( lua_manager::get_instance()->get_Lua(), -1 ) )
        {
        lua_manager::get_instance()->void_exec_lua_method( "", "init_params",
            "int tech_object_manager::init_params()" );
        }
    lua_remove( lua_manager::get_instance()->get_Lua(), -1 ); // stack: init_params

    return 0;
    }
//-----------------------------------------------------------------------------
int tech_object_manager::init_runtime_params()
    {
    for ( u_int i = 0; i < tech_objects.size(); i++ )
        {
        tech_objects[ i ]->lua_init_runtime_params();
        }

    return 0;
    }
//-----------------------------------------------------------------------------
tech_object_manager* tech_object_manager::get_instance()
    {
    if ( instance.is_null() )
        {
        instance = new tech_object_manager();
        }

    return instance;
    }
//-----------------------------------------------------------------------------
int tech_object_manager::get_object_with_active_mode( u_int mode,
    u_int start_idx, u_int end_idx )
    {
    for ( u_int i = start_idx;
        i <= end_idx && i < tech_objects.size(); i++ )
        {
        if ( tech_objects.at( i )->get_mode( mode ) ) return i;
        }

    return -1;
    }
//-----------------------------------------------------------------------------
tech_object* tech_object_manager::get_tech_objects( u_int idx )
    {
    try
        {
        return tech_objects.at( idx );
        }
    catch (...)
        {
#ifdef PAC_PC
        debug_break;
#endif // PAC_PC

        return stub;
        }
    }
//-----------------------------------------------------------------------------
u_int tech_object_manager::get_count() const
    {
    return tech_objects.size();
    }
//-----------------------------------------------------------------------------
void tech_object_manager::evaluate()
    {
    for ( u_int i = 0; i < tech_objects.size(); i++ )
        {
        tech_objects.at( i )->evaluate();
        }

    static char has_Lua_eval = 0;
    if ( has_Lua_eval == 0 )
        {
        lua_getfield( lua_manager::get_instance()->get_Lua(), LUA_GLOBALSINDEX,
            "eval" );

        if ( lua_isfunction( lua_manager::get_instance()->get_Lua(), -1 ) )
            {
            has_Lua_eval = 2;
            }
        else
            {
            has_Lua_eval = 1;
            }

        lua_remove( lua_manager::get_instance()->get_Lua(), -1 ); // stack: eval
        }

    if ( has_Lua_eval == 2 )
        {
        lua_manager::get_instance()->void_exec_lua_method( "", "eval",
            "void tech_object_manager::evaluate()" );       
        }
    }
//-----------------------------------------------------------------------------
int tech_object_manager::init_objects()
    {
    //-����� ���������������� ������� �������������.
    lua_getfield( lua_manager::get_instance()->get_Lua(), LUA_GLOBALSINDEX,
        "init" );

    if ( lua_isfunction( lua_manager::get_instance()->get_Lua(), -1 ) )
        {
        lua_manager::get_instance()->void_exec_lua_method( "", "init",
            "int tech_object_manager::init_objects()" );       
        }
    lua_remove( lua_manager::get_instance()->get_Lua(), -1 ); // stack: init
    
    return 0;
    }
//-----------------------------------------------------------------------------
tech_object_manager::~tech_object_manager()
    {
    delete stub;
    stub = 0;

// ��������� � Lua.
//    for ( u_int i = 0; i < tech_objects.size(); i++ )
//        {
//        delete tech_objects[ i ];
//        }
    }
//-----------------------------------------------------------------------------
tech_object_manager::tech_object_manager()
    {
    stub = new tech_object( "��������", 1, 1000, "STUB", 1, 1, 1, 1, 1, 1 );
    }
//-----------------------------------------------------------------------------
void tech_object_manager::add_tech_object( tech_object* new_tech_object )
    {
    tech_objects.push_back( new_tech_object );
    new_tech_object->set_serial_idx( tech_objects.size() );

    G_ERRORS_MANAGER->add_error( new tech_obj_error( new_tech_object ) );
    }
//-----------------------------------------------------------------------------
int tech_object_manager::save_params_as_Lua_str( char* str )
    {
    int res = 0;
    str[ 0 ] = 0;

    for ( u_int i = 0; i < tech_objects.size(); i++ )
        {
        res += tech_objects[ i ]->save_params_as_Lua_str( str + res );
        }
    return res;
    }
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
tech_object_manager* G_TECH_OBJECT_MNGR()
    {
    return tech_object_manager::get_instance();
    }
//-----------------------------------------------------------------------------
tech_object* G_TECH_OBJECTS( u_int idx )
    {
    return tech_object_manager::get_instance()->get_tech_objects( idx );
    }
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
