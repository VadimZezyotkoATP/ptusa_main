/// @file errors.h
/// @brief �������� �������� ������������, ������� ������������ �
///  �������� �� ������� ��� ����������� �������������� �� ������.
///
/// ����� @ref base_error, �������� ��� ����������� ������ � �������, ��������
/// ��� ����������� ����������. ��� �������� ���� ������ ������ �����
/// @ref dev_errors_manager.
///
/// @author  ������ ������� ���������.
///
/// @par �������� �������� �������������:
/// @c PAC    - ���������� ��� �����������.@n
/// @c DRIVER - ���������� ��� �������� (�� Windows).
///
/// @par ������� ������:
/// @$Rev$.\n
/// @$Author$.\n
/// @$Date::                     $.

#ifndef ERRORS_H
#define ERRORS_H

#if !( defined PAC_PC || defined PAC_WAGO_750_860 || defined UPAC_7186E || \
    defined UPAC_5000 ) && \
    !defined DRIVER
#error You must define type!
#endif

#if ( defined PAC_PC || defined PAC_WAGO_750_860 || defined UPAC_7186E || \
    defined UPAC_5000 ) && \
    !defined PAC
#define PAC
#endif

//-----------------------------------------------------------------------------
#if defined PAC || defined DRIVER
enum ALARM_STATE
    {
    AS_NORMAL,
    AS_ALARM,
    AS_RETURN,
    AS_ACCEPT,
    };

enum ALARM_TYPE
    {
    AT_DISCRETE,
    AT_VALUE,
    AT_DEVIATION,
    AT_RATE_OF_CHANGE,
    AT_SPECIAL,
    };

enum ALARM_CLASS_PRIORITY
    {
    P_FATAL         = 0,
    P_ERR_CONNECTION = 100,
    P_ALARM          = 250,
    P_MESSAGE        = 500,
    P_ANSWER         = 750,
    P_REMIND         = 1000,
    };

#endif // defined PAC || defined WIN32

//-----------------------------------------------------------------------------
#ifdef PAC
#include "param_ex.h"
#include "smart_ptr.h"
#include "PAC_dev.h"

#include "tech_def.h"

//-----------------------------------------------------------------------------
/// @brief ������� ����� � ����������� �� ������ ����������.
///
class base_error
    {
    public:
        base_error();

        virtual ~base_error()
            {
            }

        /// @brief ���������� ������ � ����� ��� �������� �� ������.
        ///
        /// @param stream       - ����� ����.
        /// @param is_new_state - ���������� �� ��������� ������
        ///                       (0 - ���, 1 - ��).
        ///
        /// @return - ���������� ���������� ����.
        virtual int save_as_Lua_str( char *str, bool &is_new_state ) = 0;

        /// @brief ���������� ����� ����������� ������ � �������.
        virtual void print() const = 0;

        /// @brief ��������� ���� �������.
        virtual unsigned char get_object_type() const = 0;

        /// @brief ��������� ������ �������.
        virtual unsigned int get_object_n() const = 0;

        /// @brief ���������� ������� ��� �������.
        virtual int set_cmd( int cmd, int object_alarm_number ) = 0;

        /// @brief ����� ���������� ������ � �������� �� ��������� (0).
        void reset_errors_params()
            {
            err_par[ P_PARAM_N ] = 0;
            }
            
        enum PARAMS  ///< ��������� ������, ���������� ������ �����.
            {
            P_PARAM_N = 1,	  //����� ���������.

            P_IS_ENABLE = 1,  ///< ���������� ������� �� ����� ��������������.
            P_IS_INHIBIT = 2, ///< ���������� ������� �� ����� ������.
            P_IS_SUPPRESS = 4,///< ���������� ������� ���������.
            };

        enum COMMANDS                ///< ���������.
            {

            C_CMD_ACCEPT   = 100,    ///< ����������� ������.
            C_CMD_SUPPRESS = 200,    ///< �������� ������.
            C_CMD_UNSET_SUPPRESS,    ///< ������ ���������� ������.
            };

    protected:
        saved_params_u_int_4 err_par;

        unsigned char error_state;    ///< ��������� ������.    
    };
//-----------------------------------------------------------------------------
/// @brief �������� ���������� �� ������ �������� ���������� (������,
/// �����...).
///
/// � �������� ���������� ����� ���� ������ ���� ������ (������ ��������
/// �����).
class simple_error: public base_error
    {
    public:
        simple_error( device* simple_device = 0 );
        virtual ~simple_error();

        /// @brief ���������� ������ � ����� ��� �������� �� ������.
        ///
        /// @param stream       - ����� ����.
        /// @param is_new_state - ���������� �� ��������� ������
        ///                       (0 - ���, 1 - ��).
        ///
        /// @return - ���������� ���������� ����.
        int save_as_Lua_str( char *str, bool &is_new_state );

        /// @brief ���������� ����� ����������� ������ � �������.
        void print() const;

        /// @brief ��������� ���� �������.
        unsigned char get_object_type() const;

        /// @brief ��������� ������ �������.
        unsigned int get_object_n() const;

        /// @brief ���������� ������� ��� �������.
        int set_cmd( int cmd, int object_alarm_number );

    private:
        device          *simple_device; ///< ������� ����������.
    };
//-----------------------------------------------------------------------------
/// @brief �������� ���������� �� ������ �������� ���������� (����,
/// ��������...).
///
/// � �������� ���������� ����� ���� ��������� ������ (���������, �����, ...).
class tech_dev_error: public base_error
    {
    public:
        // ��������� base_error.
        tech_dev_error( tech_object* tech_dev ): tech_dev( tech_dev ),
            was_set_cmd( false )
            {
            }

        int save_as_Lua_str( char *str, bool &is_new_state );

        void print() const
            {
#ifdef DEBUG
            Print( "%s\n",
                tech_dev->get_name_in_Lua() );
#endif // DEBUG
            }

        unsigned char get_object_type() const
            {
            return TE_TYPE + tech_dev->get_object_type();
            }

        unsigned int get_object_n() const
            {
            return tech_dev->get_number();
            }

        static const char* get_group( tech_object::ERR_MSG_TYPES err_type );

        static int get_priority( tech_object::ERR_MSG_TYPES err_type );

        // ����������� ��������� ��������� ����� ������� - �������� ��������� ��
        // �������, ��� �������� ���������� �������.
        int set_cmd( int cmd, int object_alarm_number );

    private:
        tech_object* tech_dev; ///< ������� ����������.

        enum TECH_DEV_ERROR_CONST   ///< ���������.
            {
            TE_TYPE = 100,          ///< ��� ������.
            };

        bool was_set_cmd;
    };
//-----------------------------------------------------------------------------
/// @brief �������� ���������� �� ���� ������� ������� ���������.
class dev_errors_manager
    {
    public:
        ~dev_errors_manager();

        //int is_any_error() const;

        /// @brief ���������� ���� ������ � ����� ��� �������� �� ������.
        ///
        /// @param stream - ����� ����.
        ///
        /// @return < 0 - ������.
        /// @return   0 - ��.
        int save_as_Lua_str( char *str, u_int_2 &id );

        /// @brief ���������� ������ � ������ ������.
        ///
        /// @param s_error - ����������� ������.
        ///
        /// @return < 0 - ������.
        /// @return   0 - ��.
        int add_error( base_error *s_error );

        /// @brief ����� ���������� ���� ������ � �������� �� ��������� (0).
        void reset_errors_params();

        /// @brief ���������� ����� ����������� ������ � �������.
        void print();

        /// @brief ��������� ���������� ������.
        void set_cmd( unsigned int cmd, unsigned int object_type,
            unsigned int object_number, unsigned int object_alarm_number );

        /// @brief ��������� ������������� ���������� ������.
        static dev_errors_manager* get_instance();

    private:
        u_int_2 errors_id; // C�������� ������.

        /// ������������ ��������� ������.
        static auto_smart_ptr < dev_errors_manager > instance;

        /// @param s_errors_cnt - ���������� ������.
        dev_errors_manager();

        enum DEM_CONST
            {
            DEM_MAX_ERRORS_CNT = 29,
            };

        std::vector< base_error* > s_errors_vector;    ///< ������ ������.
    };
//-----------------------------------------------------------------------------
#define G_DEV_ERRORS_MANAGER dev_errors_manager::get_instance()
//-----------------------------------------------------------------------------
#endif // PAC

#endif //ERRORS_H
