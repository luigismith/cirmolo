


import json
import os
from apps.pyui_app import PyUiAppConfig
from controller.controller import Controller
from controller.controller_inputs import ControllerInput
from devices.device import Device
from display.display import Display
from menus.app.app_menu_popup import AppMenuPopup
from menus.app.app_utils import AppUtils
from menus.app.hidden_apps_manager import AppsManager
from menus.language.language import Language
from themes.theme import Theme
from utils.activity.activity_tracker import ActivityTracker
from utils.boxart.box_art_scraper import BoxArtScraper
from utils.logger import PyUiLogger
from utils.py_ui_config import PyUiConfig
from utils.py_ui_state import PyUiState
from views.grid_or_list_entry import GridOrListEntry
from views.selection import Selection
from views.view_creator import ViewCreator

# Cirmolo: aree tematiche del menu App (cartelle). Le app vengono assegnate per nome della loro cartella
# in App/ (le app interne di PyUI per etichetta); quelle non elencate vanno nell'area "default".
# Senza il file, o con "appFolders": false in py-ui-config.json, resta l'elenco unico di spruce.
_PYUI_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
APP_FOLDERS_PATH = os.path.join(_PYUI_DIR, 'app-folders.json')
FOLDER_ICONS_DIR = os.path.join(_PYUI_DIR, 'folder-icons')


class AppMenu:
    def __init__(self):
        self.appFinder = Device.get_device().get_app_finder()
        self.show_all_apps = False
        self.last_folder = None

    def save_app_selection(self, selected):
        if(selected.get_selection() is not None):
            PyUiState.set_last_app_selection(selected.get_selection().get_extra_data().get_label())

    def handle_app_selection(self, app):
        launch = app.get_launch()
        folder = app.get_folder()
        Display.deinit_display()
        Device.get_device().run_app(folder,launch)
        Controller.clear_input_queue()
        Display.reinitialize()

    def append_pyui_apps(self, app_list):
        system_config = Device.get_device().get_system_config()
        if(not system_config.simple_mode_enabled()):
            boxart_scraper_config = PyUiAppConfig("Boxart Scraper")
            hidden = AppsManager.is_hidden(boxart_scraper_config) and not self.show_all_apps
            if(not hidden):
                icon = AppUtils.get_icon(None,"scraper.png")
                app_list.append(
                        GridOrListEntry(
                            primary_text=Language.app_label(boxart_scraper_config.get_label()) + Language.label("hiddenSuffix", "(Hidden)") if AppsManager.is_hidden(boxart_scraper_config) else Language.app_label(boxart_scraper_config.get_label()),
                            image_path=None,
                            image_path_selected=None,
                            description=Language.label("scrapeBoxartDesc", "Scrape game boxart"),
                            icon=icon,
                            extra_data=boxart_scraper_config,
                            value=BoxArtScraper().scrape_boxart
                        )
                )

            activity_tracker_config = PyUiAppConfig("Activity Tracker")
            hidden = AppsManager.is_hidden(activity_tracker_config) and not self.show_all_apps
            if(not hidden and PyUiConfig.get_activity_log_path() is not None):
                icon = AppUtils.get_icon(None,"rtc.png")
                app_list.append(
                        GridOrListEntry(
                            primary_text=Language.app_label(activity_tracker_config.get_label()) + Language.label("hiddenSuffix", "(Hidden)") if AppsManager.is_hidden(activity_tracker_config) else Language.app_label(activity_tracker_config.get_label()),
                            image_path=None,
                            image_path_selected=None,
                            description=Language.label("trackAppUsageDesc", "Track app usage"),
                            icon=icon,
                            extra_data=activity_tracker_config,
                            value=ActivityTracker().run_activity_tracking_app
                        )
                )

    def app_entries(self):
        system_config = Device.get_device().get_system_config()
        app_list = []
        device_apps = self.appFinder.get_apps()
        for app in device_apps:
            hidden = AppsManager.is_hidden(app) and not self.show_all_apps
            devices = app.get_devices()
            supported_device = Device.supports_device(devices)
            allowed_in_mode = not system_config.simple_mode_enabled() or not app.get_hide_in_simple_mode()
            if(allowed_in_mode and app.get_label() is not None and not hidden and supported_device):
                icon = AppUtils.get_icon(app.get_folder(),app.get_icon())
                app_list.append(
                    GridOrListEntry(
                        primary_text=Language.app_label(app.get_label()) + Language.label("hiddenSuffix", "(Hidden)") if AppsManager.is_hidden(app) else Language.app_label(app.get_label()),
                        image_path=icon,
                        image_path_selected=icon,
                        description=Language.app_description(app.get_description()),
                        icon=icon,
                        extra_data=app,
                        value=lambda app=app: self.handle_app_selection(app)
                    )
                )

        self.append_pyui_apps(app_list)
        app_list.sort(key=lambda app: app.get_primary_text() or "")
        return app_list

    @staticmethod
    def load_folders():
        if(not PyUiConfig.get("appFolders", True)):
            return None
        try:
            with open(APP_FOLDERS_PATH, encoding="utf-8") as f:
                data = json.load(f)
        except Exception as e:
            PyUiLogger.get_logger().info(f"App folders not used: {e}")
            return None
        if(not isinstance(data, dict) or not isinstance(data.get("folders"), list)):
            return None
        return data

    @staticmethod
    def folder_key(app, data):
        default = data.get("default", "other")
        if(app is None):
            return default
        if(isinstance(app, PyUiAppConfig)):
            return data.get("pyui", {}).get(app.get_label(), default)
        folder = app.get_folder()
        name = os.path.basename(os.path.normpath(folder)) if folder else None
        return data.get("apps", {}).get(name, default)

    @staticmethod
    def folder_definitions(data):
        defs = [d for d in data.get("folders", []) if isinstance(d, dict) and d.get("key")]
        default = data.get("default", "other")
        if(not any(d.get("key") == default for d in defs)):
            defs.append({"key": default, "label": "Other", "icon": "altro.png"})
        return defs

    def run_app_selection(self) :
        data = self.load_folders()
        if(data is None):
            return self.run_app_list(None, None)

        view = None
        while(True):
            groups = {}
            for entry in self.app_entries():
                groups.setdefault(self.folder_key(entry.get_extra_data(), data), []).append(entry)

            folder_entries = []
            for d in self.folder_definitions(data):
                items = groups.get(d.get("key"))
                if(not items):
                    continue
                names = ", ".join(i.get_primary_text() for i in items[:4]) + (", ..." if len(items) > 4 else "")
                icon = AppUtils.get_icon(None, os.path.join(FOLDER_ICONS_DIR, d.get("icon", "altro.png")))
                folder_entries.append(
                    GridOrListEntry(
                        primary_text=Language.translate("appFolders", d.get("label", d.get("key"))),
                        image_path=icon,
                        image_path_selected=icon,
                        description=names,
                        icon=icon,
                        extra_data=None,
                        value=d.get("key")
                    )
                )
            if(not folder_entries):
                return self.run_app_list(None, None)

            if(view is None):
                index = next((i for i, f in enumerate(folder_entries) if f.get_value() == self.last_folder), 0)
                view = ViewCreator.create_view(
                    view_type=Theme.get_view_type_for_app_menu(),
                    top_bar_text=Language.apps(),
                    options=folder_entries,
                    selected_index=index)
            else:
                view.set_options(folder_entries)

            selected = Selection(None, None, None)
            while(selected.get_input() is None):
                selected = view.get_selection(select_controller_inputs = [ControllerInput.A, ControllerInput.MENU])
                if(ControllerInput.A == selected.get_input() and selected.get_selection() is not None):
                    self.last_folder = selected.get_selection().get_value()
                    result = self.run_app_list(self.last_folder, data, selected.get_selection().get_primary_text())
                    if(result in (ControllerInput.L1, ControllerInput.R1)):
                        return result
                elif(ControllerInput.B == selected.get_input()):
                    return None
                elif(ControllerInput.MENU == selected.get_input()):
                    self.show_all_apps = AppMenuPopup(self.show_all_apps).run_app_menu_popup(None)
                elif(Theme.skip_main_menu() and selected.get_input() in (ControllerInput.L1, ControllerInput.R1)):
                    return selected.get_input()

    def run_app_list(self, folder, data, title=None) :
        running = True
        view = None

        while(running):
            last_selected_label = PyUiState.get_last_app_selection()
            selected = Selection(None,None,0)
            app_list = self.app_entries()
            if(folder is not None):
                app_list = [e for e in app_list if self.folder_key(e.get_extra_data(), data) == folder]
                if(not app_list):
                    return None

            idx = 0
            for app in app_list:
                if(app.get_extra_data() is not None and app.get_extra_data().get_label() == last_selected_label):
                    selected = Selection(None,None,idx)
                    break
                idx += 1

            PyUiLogger.get_logger().info(f"Finish app list building")

            if(view is None):
                view = ViewCreator.create_view(
                    view_type=Theme.get_view_type_for_app_menu(),
                    top_bar_text=title if title else Language.apps(),
                    options=app_list,
                    selected_index=selected.get_index())
            else:
                view.set_options(app_list)

            selected = Selection(None, None, None)
            while(selected.get_input() is None):
                selected = view.get_selection(select_controller_inputs = [ControllerInput.A, ControllerInput.MENU])
                if(ControllerInput.A == selected.get_input()):
                    self.save_app_selection(selected)
                    selected.get_selection().get_value()()
                elif(ControllerInput.B == selected.get_input()):
                    self.save_app_selection(selected)
                    running = False
                elif(ControllerInput.MENU == selected.get_input()):
                    self.save_app_selection(selected)
                    if(selected.get_selection()):
                        self.show_all_apps = AppMenuPopup(self.show_all_apps).run_app_menu_popup(selected.get_selection().get_extra_data())
                    else:
                        self.show_all_apps = AppMenuPopup(self.show_all_apps).run_app_menu_popup(None)
                elif(Theme.skip_main_menu() and ControllerInput.L1 == selected.get_input()):
                    self.save_app_selection(selected)
                    return ControllerInput.L1
                elif(Theme.skip_main_menu() and ControllerInput.R1 == selected.get_input()):
                    self.save_app_selection(selected)
                    return ControllerInput.R1


