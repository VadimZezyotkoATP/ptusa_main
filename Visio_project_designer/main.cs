﻿/// @file main.cs
/// @brief Системный класс, который реализует дополнение к Visio.
/// 
/// @author  Иванюк Дмитрий Сергеевич.
/// 
/// @par Текущая версия:
/// @$Rev$.\n
/// @$Author$.\n
/// @$Date::                     $.
/// 
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Xml.Linq;
using Visio  = Microsoft.Office.Interop.Visio;
using Office = Microsoft.Office.Core;

using System.Windows.Forms;
using System.Text.RegularExpressions;

using wago;
using tech_device;

/// Пространство имен данного дополнения к Visio.
namespace visio_prj_designer
    {
    /// <summary> Класс дополнения. </summary>
    ///
    /// <remarks> Id, 16.08.2011. </remarks>
    public partial class visio_addin
        {
        /// <summary> Флаг режима привязки канала к клемме модуля. </summary>
        internal bool is_selecting_clamp = false;

        /// <summary> Лента дополнения. </summary>
        internal main_ribbon vis_main_ribbon;

        /// <summary> Все устройства. </summary>
        internal List<device> g_devices;

        /// <summary> Объект контроллера. </summary>
        internal PAC g_PAC = null;       

        /// <summary> Флаг режима привязки устройств к модулям. </summary>
        internal bool is_device_edit_mode = false; 

        /// <summary> Форма редактирования привязки к каналам. </summary>
        internal edit_valve_type_form_usage edit_io_frm;

        /// <summary> Константы для обращения к окнам Visio. </summary>
        public enum VISIO_WNDOWS : short
            {
            MAIN    = 1, ///< Главное окно.
            IO_EDIT = 2, ///< Дополнительное окно.
            };

        /// <summary> Приложение Visio. </summary>
        private Visio.Application visio_app;

        /// <summary> Event handler. Called by <c>visio_addin</c> for startup 
        /// events. Вызывается при загрузке дополнения. Здесь мы регистрируем 
        /// свои обработчики событий, создаем нужные объекты. </summary>
        ///
        /// <remarks> Id, 16.08.2011. </remarks>
        ///
        /// <param name="sender"> Source of the event. </param>
        /// <param name="e">      Event information. </param>
        private void visio_addin__startup( object sender, System.EventArgs e )
            {

            visio_app = this.Application;
            visio_app.DocumentCreated +=
                new Microsoft.Office.Interop.Visio.EApplication_DocumentCreatedEventHandler(
                    visio_addin__DocumentCreated );

            visio_app.ShapeAdded +=
                new Microsoft.Office.Interop.Visio.EApplication_ShapeAddedEventHandler(
                    visio_addin__ShapeAdded );

            visio_app.BeforeShapeDelete +=
                new Microsoft.Office.Interop.Visio.EApplication_BeforeShapeDeleteEventHandler(
                    visio_addin__BeforeShapeDelete );

            visio_app.ConnectionsAdded +=
                new Microsoft.Office.Interop.Visio.EApplication_ConnectionsAddedEventHandler(
                    visio_addin__ConnectionsAdded );

            visio_app.FormulaChanged += new Microsoft.Office.Interop.Visio.EApplication_FormulaChangedEventHandler(
                    visio_addin__FormulaChanged );


            visio_app.SelectionChanged += new Microsoft.Office.Interop.Visio.EApplication_SelectionChangedEventHandler(
                    visio_addin__SelectionChanged );

            visio_app.DocumentOpened += new Microsoft.Office.Interop.Visio.EApplication_DocumentOpenedEventHandler(
                 visio_addin__DocumentOpened );

            visio_app.MouseDown +=new Visio.EApplication_MouseDownEventHandler(
                 visio_addin___MouseDown );

            g_devices = new List< device > ();
            }

        /// <summary> Event handler. Called by visio_addin for shutdown events. </summary>
        ///
        /// <remarks> Id, 16.08.2011. </remarks>
        ///
        /// <param name="sender"> Source of the event. </param>
        /// <param name="e">      Event information. </param>
        private void visio_addin__shutdown( object sender, System.EventArgs e )
            {
            }

        /// <summary> Event handler. Реализована обработка клика мыши
        /// в режиме выбора клеммы для привязки. </summary>
        ///
        /// <remarks> Id, 16.08.2011. </remarks>
        ///
        /// <param name="Button">         The button. </param>
        /// <param name="KeyButtonState"> State of the key button. </param>
        /// <param name="x">              The x coordinate. </param>
        /// <param name="y">              The y coordinate. </param>
        /// <param name="CancelDefault">  [in,out] The cancel default. </param>
        private void visio_addin___MouseDown( int Button, int KeyButtonState, double x,
            double y, ref bool CancelDefault )
            {
            if( is_selecting_clamp )
                {
                foreach( io_module module in g_PAC.get_io_modules() )
                    {
                    int clamp = module.get_mouse_selection();
                    if( clamp > -1 )
                        {
                        g_PAC.unmark();

                        current_selected_dev.set_channel( 
                            current_selected_dev.get_active_channel(), module, clamp );

                        visio_addin__SelectionChanged( visio_app.Windows[ 
                            ( short ) visio_addin.VISIO_WNDOWS.IO_EDIT ] );

                        visio_app.Windows[ ( short ) visio_addin.VISIO_WNDOWS.MAIN ].MouseMove -=
                            new Microsoft.Office.Interop.Visio.EWindow_MouseMoveEventHandler(
                            Globals.visio_addin.visio_addin__MouseMove );
                        is_selecting_clamp = false;

                        CancelDefault = true;

                        return;
                        }
                    }

                g_PAC.unmark();
                visio_addin__SelectionChanged( visio_app.Windows[
                    ( short ) visio_addin.VISIO_WNDOWS.IO_EDIT ] );

                visio_app.Windows[ ( short ) visio_addin.VISIO_WNDOWS.MAIN ].MouseMove -=
                    new Microsoft.Office.Interop.Visio.EWindow_MouseMoveEventHandler(
                    Globals.visio_addin.visio_addin__MouseMove );
                is_selecting_clamp = false;
                }
            }

        /// <summary> Event handler. Реализована обработка перемещения мыши
        /// в режиме выбора клеммы для привязки. </summary>
        ///
        /// <remarks> Id, 16.08.2011. </remarks>
        ///
        /// <param name="Button">         The button. </param>
        /// <param name="KeyButtonState"> State of the key button. </param>
        /// <param name="x">              The x coordinate. </param>
        /// <param name="y">              The y coordinate. </param>
        /// <param name="CancelDefault">  [in,out] The cancel default. </param>
        internal void visio_addin__MouseMove( int Button,
            int KeyButtonState, double x, double y, ref bool CancelDefault )
            {

            try
                {
                if( is_selecting_clamp )
                    {
                    foreach( io_module module in g_PAC.get_io_modules() )
                        {
                        module.magnify_on_mouse_move( x, y );
                        }
                    }

                }
            catch( System.Exception ex )
                {
                MessageBox.Show( ex.Message );
                }
            }

        /// <summary> Event handler. Создание необходимых объектов при открытии
        /// файла. </summary>
        ///
        /// <remarks> Id, 16.08.2011. </remarks>
        ///
        /// <param name="target"> Target for the. </param>
        private void visio_addin__DocumentOpened( Visio.Document target )
            {
            if( target.Type == Visio.VisDocumentTypes.visTypeDrawing )
                {
                if( target.Template.Contains( "PTUSA project" ) )
                    {
                    vis_main_ribbon.show();

                    //Считывание устройств Wago.
                    foreach( Visio.Shape shape in target.Pages[ "Wago" ].Shapes )
                        {
                        if( shape.Data1 == "750" && shape.Data2 == "860" )
                            {
                            string ip_addr = shape.Cells[ "Prop.ip_address" ].Formula;
                            string name = shape.Cells[ "Prop.PAC_name" ].Formula;

                            g_PAC = new PAC( name.Substring( 1, name.Length - 2 ),
                                ip_addr.Substring( 1, ip_addr.Length - 2 ), shape );

                            }

                        if( shape.Data1 == "750" && shape.Data2 != "860" )
                            {
                            g_PAC.add_io_module( shape );
                            }
                        }

                    //Считывание устройств.
                    foreach( Visio.Shape shape in target.Pages[ "Устройства" ].Shapes )
                        {
                        if( shape.Data1 == "V" )
                            {
                            g_devices.Add( new device( shape, g_PAC ) );
                            }
                        }
                    }
                else
                    {
                    vis_main_ribbon.hide();
                    }
                }
            }

        /// <summary> Предыдущее активное устройство (выделенное мышкой). </summary>
        private device previous_selected_dev = null;

        /// <summary> Текущее активное устройство (выделенное мышкой). </summary>
        internal device current_selected_dev = null;

        /// <summary> Event handler. Обработка изменения активного устройства 
        /// (подсветка клемм устройства и т.д.). </summary>
        ///
        /// <remarks> Id, 16.08.2011. </remarks>
        ///
        /// <param name="window"> Активное окно. </param>
        private void visio_addin__SelectionChanged( Microsoft.Office.Interop.Visio.Window window )
            {

            // Проверка на режим привязки устройств к каналам ввода\вывода.
            if( Globals.visio_addin.is_device_edit_mode ) 
                {
                // Проверка на активность окна редактирования.
                if( window.Index == ( short ) VISIO_WNDOWS.IO_EDIT )
                    {

                    // Проверка на активность страницы с устройствами (клапанами т.д.).
                    if( window.Page.Name == "Устройства" )
                        {

                        const int IO_PROP_WINDOW_INDEX = 8;

                        window.Windows[ IO_PROP_WINDOW_INDEX ].Caption =
                                "Каналы";

                        if( window.Selection.Count > 0 )
                            {
                            window.Windows[ IO_PROP_WINDOW_INDEX ].Caption =
                                window.Windows[ IO_PROP_WINDOW_INDEX ].Caption +
                                " - " + window.Selection[ 1 ].Name;

                            //Снятие ранее подсвеченных модулей Wago.
                            if( previous_selected_dev != null )
                                {
                                previous_selected_dev.unselect_channels();
                                }

                            Microsoft.Office.Interop.Visio.Shape selected_shape =
                                window.Selection[ 1 ];

                            //Поиск по shape объекта device.
                            int dev_n    = Convert.ToInt16( selected_shape.Cells[ "Prop.number" ].FormulaU );
                            int dev_type = Convert.ToInt16( selected_shape.Cells[ "Prop.type" ].FormulaU );

                            current_selected_dev = g_devices.Find( delegate( device dev )
                                {
                                return dev.get_n() == dev_n && dev.get_type() == dev_type; 
                                }
                                );

                            if( current_selected_dev != null )
                                {
                                current_selected_dev.select_channels();

                                //Обновление окна со свойствами привязки.
                                edit_io_frm.listForm.enable_prop();

                                current_selected_dev.refresh_edit_window(
                                    edit_io_frm.listForm.type_cbox,
                                    edit_io_frm.listForm.type_lview );                                                               
                                }

                                previous_selected_dev = current_selected_dev;

                            } //if( window.Selection.Count > 0 )
                        else
                            {
                            //Снятие ранее подсвеченных модулей Wago.
                            if( previous_selected_dev != null )
                                {
                                previous_selected_dev.unselect_channels();
                                previous_selected_dev = null;
                                }

                            edit_io_frm.listForm.disable_prop();
                            }

                        } //if( window.Page.Name == "Устройства" )
                    } //if( window.Index == ( short ) VISIO_WNDOWS.IO_EDIT )
                } //if( visio_addin.is_device_edit_mode ) 
            }

        /// <summary> Event handler. Обработчик изменения формул (задание 
        /// вычисляемых свойств объекта (порядковый номер, ...) при его 
        /// добавлении и т.д.). </summary>
        ///
        /// <remarks> Id, 16.08.2011. </remarks>
        ///
        /// <param name="cell"> Ячейка, где произошло изменение. </param>
        private void visio_addin__FormulaChanged( Microsoft.Office.Interop.Visio.Cell cell )
            {
            if( cell.Shape.Data1 == "750" )
                {
                if( cell.Name == "Prop.order_number" )
                    {
                    cell.Shape.Shapes[ "type_skin" ].Shapes[ "module_number" ].Text =
                        cell.Shape.Cells[ "Prop.order_number" ].Formula;
                    return;
                    }

                if( cell.Name == "Prop.type" )
                    {
                    int type = ( int ) Convert.ToDouble( cell.Shape.Cells[ "Prop.type" ].ResultStr[ "" ] );

                    cell.Shape.Data2 = Convert.ToString( type );
                    cell.Shape.Shapes[ "type_str" ].Text = Convert.ToString( type );

                    string colour = "1";
                    switch( type )
                        {
                        case 461:
                        case 466:
                            colour = "RGB( 180, 228, 180 )";
                            break;

                        case 504:
                        case 512:
                            colour = "RGB( 255, 128, 128 )";
                            break;

                        case 402:
                            colour = "RGB( 255, 255, 128 )";
                            break;

                        case 602:
                        case 612:
                        case 638:
                            colour = "14";
                            break;

                        case 600:
                            colour = "15";
                            break;
                        }

                    cell.Shape.Shapes[ "type_skin" ].Shapes[ "module_number" ].Cells[ 
                        "FillForegnd" ].FormulaU = colour;
                    cell.Shape.Shapes[ "type_skin" ].Shapes[ "3" ].Cells[
                        "FillForegnd" ].FormulaU = colour;
                    }
                }

            if( cell.Shape.Data1 == "V" )
                {
                switch( cell.Name )
                    {
                    //case "Prop.type":
                    //    string type = cell.Shape.Cells[ "Prop.type" ].get_ResultStr( 0 );

                    //    switch( type )
                    //        {
                    //        case "1 КУ":
                    //            //cell.Shape.AddSection( cell.Shape.get_CellsRowIndex );
                    //            int idx = cell.Shape.get_CellsRowIndex( "Prop.type" );

                    //            cell.Shape.AddNamedRow(
                    //                ( short ) Visio.VisSectionIndices.visSectionUser, "DO",
                    //                ( short ) Visio.VisRowTags.visTagDefault );

                    //            cell.Shape.AddNamedRow(
                    //                ( short ) Visio.VisSectionIndices.visSectionProp, "hf",
                    //                ( short ) 0 );


                    //            break;
                    //        }

                    //    break;

                    case "Prop.name":
                        string str = cell.Shape.Cells[ "Prop.name" ].Formula;

                        //Первый вариант маркировки клапана - 1V12.
                        Regex rex = new Regex( @"\b([0-9]{0,4})V([0-9]{1,2})\b",
                            RegexOptions.IgnoreCase );
                        if( rex.IsMatch( str ) )
                            {
                            Match mtc = rex.Match( str );

                            string n_part_1 = mtc.Groups[ 1 ].ToString();
                            string n_part_2 = mtc.Groups[ 2 ].ToString();
                            if( n_part_1 == "" ) n_part_1 = "0";

                            int n = Convert.ToUInt16( n_part_1 ) * 100 +
                                Convert.ToUInt16( n_part_2 );

                            cell.Shape.Cells[ "Prop.number" ].FormulaU = n.ToString();

                            str = str.Replace( "\"", "" );
                            cell.Shape.Name = str;
                            cell.Shape.Shapes[ "name" ].Text = str;
                            break;
                            }

                        //Второй вариант маркировки клапана - S4V12.
                        Regex rex2 = new Regex( @"\b([a-z]{1})([0-9]{1})V([0-9]{1,2})\b",
                            RegexOptions.IgnoreCase );
                        if( rex2.IsMatch( str ) )
                            {
                            Match mtc = rex2.Match( str );

                            //Линия (A-W).
                            int n = 100 * ( 200 +
                                ( Convert.ToUInt16( mtc.Groups[ 1 ].ToString()[ 0 ] ) - 65 ) * 20 );
                            //Номер линии (0-19).
                            n += 100 * Convert.ToUInt16( mtc.Groups[ 2 ].ToString() );
                            //Клапан.
                            n += Convert.ToUInt16( mtc.Groups[ 3 ].ToString() );

                            cell.Shape.Cells[ "Prop.number" ].FormulaU = n.ToString();

                            str = str.Replace( "\"", "" );
                            cell.Shape.Name = str;                                                        
                            cell.Shape.Shapes[ "name" ].Text = str;
                            break;
                            }

                        MessageBox.Show( "Неверная маркировка клапана - \"" +
                            str + "\"!" );
                        cell.Shape.Cells[ "Prop.name" ].FormulaU = "\"V19\"";
                        break;
                    }
                }
            }

        /// <summary> Event handler. Обработчик события "привязки" фигуры. Здесь
        /// заполняем порядковый номер модуля в наборе. </summary>
        ///
        /// <remarks> Id, 16.08.2011. </remarks>
        ///
        /// <param name="connect"> Объект "склейки". </param>
        private void visio_addin__ConnectionsAdded( Microsoft.Office.Interop.Visio.Connects connect )
            {
            Microsoft.Office.Interop.Visio.Shape obj_2 =
                visio_app.ActivePage.Shapes[ connect.FromSheet.Name ];
            Microsoft.Office.Interop.Visio.Shape obj_1 =
                visio_app.ActivePage.Shapes[ connect.ToSheet.Name ];

            switch( obj_1.Data1 )
                {
                case "750":
                    if( obj_1.Data2 == "860" )
                        {
                        obj_2.Cells[ "Prop.order_number" ].FormulaU = "1";
                        }
                    else
                        {
                        int new_number = Convert.ToInt32( obj_1.Cells[ "Prop.order_number" ].FormulaU ) + 1;
                        obj_2.Cells[ "Prop.order_number" ].FormulaU = Convert.ToString( new_number );
                        }

                    break;
                }
            }

        /// <summary> Event handler. Удаляем соответствующий объект при удалении
        /// связанной фигуры. </summary>
        ///
        /// <remarks> Id, 16.08.2011. </remarks>
        ///
        /// <param name="shape"> Удаляемая фигура. </param>
        private void visio_addin__BeforeShapeDelete( Microsoft.Office.Interop.Visio.Shape shape )
            {
            switch( shape.Data1 )
                {
                case "750":
                    if( shape.Data2 == "860" )
                        {
                        if( no_delete_g_pac_flag )
                            {
                            no_delete_g_pac_flag = false;
                            }
                        else
                            {
                            g_PAC = null;
                            }
                        }
                    break;
                }
            }

        #region Поля для реализации функциональности вставки сразу нескольких модулей Wago.

        private bool is_duplicating = false;
        private int duplicate_count = 0;

        private Microsoft.Office.Interop.Visio.Shape old_shape;
        private Microsoft.Office.Interop.Visio.Shape new_shape;

        private bool no_delete_g_pac_flag = false;
        
        #endregion

        /// <summary> Event handler. Обработка события добавления фигуры. Здесь
        /// реализована часть функциональности вставки сразу нескольких модулей 
        /// Wago. </summary>
        ///
        /// <remarks> Id, 16.08.2011. </remarks>
        ///
        /// <param name="shape"> Добавляемая фигура. </param>
        private void visio_addin__ShapeAdded( Microsoft.Office.Interop.Visio.Shape shape )
            {
            //Проверяем, нужный ли нам объект мы добавляем на лист. Выводим окно
            //с запросом количества элементов. Помещаем на форму первый элемент,
            //остальные приклеиваем к нему путем дублирования вставляемого 
            //элемента.

            if( is_duplicating )
                {

                duplicate_count--;
                if( duplicate_count <= 0 )
                    {
                    is_duplicating = false;
                    }

                return;
                }

            if( shape.Data1 == "750" && shape.Data2 == "860" )
                {
                if( g_PAC == null )
                    {
                    string ip_addr = shape.Cells[ "Prop.ip_address" ].Formula;
                    string name = shape.Cells[ "Prop.PAC_name" ].Formula;

                    g_PAC = new PAC( name.Substring( 1, name.Length - 2 ),
                        ip_addr.Substring( 1, ip_addr.Length - 2 ), shape );
                    }
                else
                    {
                    no_delete_g_pac_flag = true;
                    MessageBox.Show( "Только один контроллер может быть в проекте!" );
                    shape.DeleteEx( 0 );
                    }
                return;
                }

            if( shape.Data1 == "750" && shape.Data2 != "860" )
                {
                modules_count_enter modules_count_enter_form = new modules_count_enter();
                modules_count_enter_form.ShowDialog();
                duplicate_count = modules_count_enter_form.modules_count - 1;


                g_PAC.add_io_module( shape );
                
                shape.Cells[ "Prop.type" ].FormulaU = 
                    modules_count_enter_form.modules_type;                                

                for( int i = 0; i < duplicate_count; i++ )
                    {
                    is_duplicating = true;
                    new_shape = shape.Duplicate();
                    old_shape = shape;
                                        
                    string str_x = string.Format( "PNTX(LOCTOPAR(PNT('{0}'!Connections.X2,'{0}'!Connections.Y2),'{0}'!EventXFMod,EventXFMod))+6 mm",
                        old_shape.Name );
                    string str_y = string.Format( "PNTY(LOCTOPAR(PNT('{0}'!Connections.X2,'{0}'!Connections.Y2),'{0}'!EventXFMod,EventXFMod))+-47 mm",
                        old_shape.Name );

                    new_shape.Cells[ "PinX" ].Formula = str_x;
                    new_shape.Cells[ "PinY" ].Formula = str_y;

                    shape = new_shape;

                    g_PAC.add_io_module( shape );
                    visio_addin__FormulaChanged( shape.Cells[ "Prop.type" ] );
                    }
                }

            if( shape.Data1 == "V" )
                {
                g_devices.Add( new device( shape, g_PAC ) );
                }
            }

        /// <summary> Event handler. При создании документа показываем полосу 
        /// работы с техпроцессом. </summary>
        ///
        /// <remarks> Id, 16.08.2011. </remarks>
        ///
        /// <param name="doc"> Created document. </param>
        private void visio_addin__DocumentCreated( Microsoft.Office.Interop.Visio.Document doc )
            {
            if( doc.Template.Contains( "PTUSA project" ) )
                {
                vis_main_ribbon.show();
                }
            else
                {
                vis_main_ribbon.hide();
                }
            }

        #region VSTO generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InternalStartup()
            {
            this.Startup += new System.EventHandler( visio_addin__startup );
            this.Shutdown += new System.EventHandler( visio_addin__shutdown );
            }

        #endregion
        }
    }