// Copyright (c) 2014 GitHub, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "browser/native_window_gtk.h"

#include "base/values.h"
#include "common/options_switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/renderer_preferences.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/skia_utils_gtk.h"

namespace atom {

namespace {

// Dividing GTK's cursor blink cycle time (in milliseconds) by this value yields
// an appropriate value for content::RendererPreferences::caret_blink_interval.
// This matches the logic in the WebKit GTK port.
const double kGtkCursorBlinkCycleFactor = 2000.0;

}  // namespace

NativeWindowGtk::NativeWindowGtk(content::WebContents* web_contents,
                                 base::DictionaryValue* options)
    : NativeWindow(web_contents, options),
      window_(GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL))),
      fullscreen_(false),
      is_always_on_top_(false) {
  gtk_container_add(GTK_CONTAINER(window_),
                    GetWebContents()->GetView()->GetNativeView());

  int width = 800, height = 600;
  options->GetInteger(switches::kWidth, &width);
  options->GetInteger(switches::kHeight, &height);
  gtk_window_set_default_size(window_, width, height);

  if (!has_frame_)
    gtk_window_set_decorated(window_, false);

  if (!icon_.IsEmpty())
    gtk_window_set_icon(window_, icon_.ToGdkPixbuf());

  g_signal_connect(window_, "delete-event",
                   G_CALLBACK(OnWindowDeleteEventThunk), this);
  g_signal_connect(window_, "focus-out-event",
                   G_CALLBACK(OnFocusOutThunk), this);

  SetWebKitColorStyle();
}

NativeWindowGtk::~NativeWindowGtk() {
  if (window_)
    gtk_widget_destroy(GTK_WIDGET(window_));
}

void NativeWindowGtk::Close() {
  CloseWebContents();
}

void NativeWindowGtk::CloseImmediately() {
  gtk_widget_destroy(GTK_WIDGET(window_));
  window_ = NULL;
}

void NativeWindowGtk::Move(const gfx::Rect& pos) {
  gtk_window_move(window_, pos.x(), pos.y());
  gtk_window_resize(window_, pos.width(), pos.height());
}

void NativeWindowGtk::Focus(bool focus) {
  if (focus)
    gtk_window_present(window_);
  else
    gdk_window_lower(gtk_widget_get_window(GTK_WIDGET(window_)));
}

bool NativeWindowGtk::IsFocused() {
  return gtk_window_is_active(window_);
}

void NativeWindowGtk::Show() {
  gtk_widget_show_all(GTK_WIDGET(window_));
}

void NativeWindowGtk::Hide() {
  gtk_widget_hide(GTK_WIDGET(window_));
}

bool NativeWindowGtk::IsVisible() {
  return gtk_widget_get_visible(GTK_WIDGET(window_));
}

void NativeWindowGtk::Maximize() {
  gtk_window_maximize(window_);
}

void NativeWindowGtk::Unmaximize() {
  gtk_window_unmaximize(window_);
}

void NativeWindowGtk::Minimize() {
  gtk_window_iconify(window_);
}

void NativeWindowGtk::Restore() {
  gtk_window_present(window_);
}

void NativeWindowGtk::SetFullscreen(bool fullscreen) {
  fullscreen_ = fullscreen;
  if (fullscreen)
    gtk_window_fullscreen(window_);
  else
    gtk_window_unfullscreen(window_);
}

bool NativeWindowGtk::IsFullscreen() {
  return fullscreen_;
}

void NativeWindowGtk::SetSize(const gfx::Size& size) {
  gtk_window_resize(window_, size.width(), size.height());
}

gfx::Size NativeWindowGtk::GetSize() {
  GdkWindow* gdk_window = gtk_widget_get_window(GTK_WIDGET(window_));

  GdkRectangle frame_extents;
  gdk_window_get_frame_extents(gdk_window, &frame_extents);

  return gfx::Size(frame_extents.width, frame_extents.height);
}

void NativeWindowGtk::SetMinimumSize(const gfx::Size& size) {
  minimum_size_ = size;

  GdkGeometry geometry = { 0 };
  geometry.min_width = size.width();
  geometry.min_height = size.height();
  int hints = GDK_HINT_POS | GDK_HINT_MIN_SIZE;
  gtk_window_set_geometry_hints(
      window_, GTK_WIDGET(window_), &geometry, (GdkWindowHints)hints);
}

gfx::Size NativeWindowGtk::GetMinimumSize() {
  return minimum_size_;
}

void NativeWindowGtk::SetMaximumSize(const gfx::Size& size) {
  maximum_size_ = size;

  GdkGeometry geometry = { 0 };
  geometry.max_width = size.width();
  geometry.max_height = size.height();
  int hints = GDK_HINT_POS | GDK_HINT_MAX_SIZE;
  gtk_window_set_geometry_hints(
      window_, GTK_WIDGET(window_), &geometry, (GdkWindowHints)hints);
}

gfx::Size NativeWindowGtk::GetMaximumSize() {
  return maximum_size_;
}

void NativeWindowGtk::SetResizable(bool resizable) {
  // Should request widget size after setting unresizable, otherwise the
  // window will shrink to a very small size.
  if (!IsResizable()) {
    gint width, height;
    gtk_window_get_size(window_, &width, &height);
    gtk_widget_set_size_request(GTK_WIDGET(window_), width, height);
  }

  gtk_window_set_resizable(window_, resizable);
}

bool NativeWindowGtk::IsResizable() {
  return gtk_window_get_resizable(window_);
}

void NativeWindowGtk::SetAlwaysOnTop(bool top) {
  is_always_on_top_ = top;
  gtk_window_set_keep_above(window_, top ? TRUE : FALSE);
}

bool NativeWindowGtk::IsAlwaysOnTop() {
  return is_always_on_top_;
}

void NativeWindowGtk::Center() {
  gtk_window_set_position(window_, GTK_WIN_POS_CENTER);
}

void NativeWindowGtk::SetPosition(const gfx::Point& position) {
  gtk_window_move(window_, position.x(), position.y());
}

gfx::Point NativeWindowGtk::GetPosition() {
  GdkWindow* gdk_window = gtk_widget_get_window(GTK_WIDGET(window_));

  GdkRectangle frame_extents;
  gdk_window_get_frame_extents(gdk_window, &frame_extents);

  return gfx::Point(frame_extents.x, frame_extents.y);
}

void NativeWindowGtk::SetTitle(const std::string& title) {
  gtk_window_set_title(window_, title.c_str());
}

std::string NativeWindowGtk::GetTitle() {
  return gtk_window_get_title(window_);
}

void NativeWindowGtk::FlashFrame(bool flash) {
  gtk_window_set_urgency_hint(window_, flash);
}

void NativeWindowGtk::SetKiosk(bool kiosk) {
  SetFullscreen(kiosk);
}

bool NativeWindowGtk::IsKiosk() {
  return IsFullscreen();
}

bool NativeWindowGtk::HasModalDialog() {
  // FIXME(zcbenz): Implement me.
  return false;
}

gfx::NativeWindow NativeWindowGtk::GetNativeWindow() {
  return window_;
}

void NativeWindowGtk::UpdateDraggableRegions(
    const std::vector<DraggableRegion>& regions) {
}

void NativeWindowGtk::SetWebKitColorStyle() {
  content::RendererPreferences* prefs =
      GetWebContents()->GetMutableRendererPrefs();
  GtkStyle* frame_style = gtk_rc_get_style(GTK_WIDGET(window_));
  prefs->focus_ring_color =
      gfx::GdkColorToSkColor(frame_style->bg[GTK_STATE_SELECTED]);
  prefs->thumb_active_color = SkColorSetRGB(244, 244, 244);
  prefs->thumb_inactive_color = SkColorSetRGB(234, 234, 234);
  prefs->track_color = SkColorSetRGB(211, 211, 211);

  GtkWidget* url_entry = gtk_entry_new();
  GtkStyle* entry_style = gtk_rc_get_style(url_entry);
  prefs->active_selection_bg_color =
      gfx::GdkColorToSkColor(entry_style->base[GTK_STATE_SELECTED]);
  prefs->active_selection_fg_color =
      gfx::GdkColorToSkColor(entry_style->text[GTK_STATE_SELECTED]);
  prefs->inactive_selection_bg_color =
      gfx::GdkColorToSkColor(entry_style->base[GTK_STATE_ACTIVE]);
  prefs->inactive_selection_fg_color =
      gfx::GdkColorToSkColor(entry_style->text[GTK_STATE_ACTIVE]);
  gtk_widget_destroy(url_entry);

  const base::TimeDelta cursor_blink_time = gfx::GetCursorBlinkCycle();
  prefs->caret_blink_interval =
      cursor_blink_time.InMilliseconds() ?
      cursor_blink_time.InMilliseconds() / kGtkCursorBlinkCycleFactor :
      0;
}

gboolean NativeWindowGtk::OnWindowDeleteEvent(GtkWidget* widget,
                                              GdkEvent* event) {
  Close();
  return TRUE;
}

gboolean NativeWindowGtk::OnFocusOut(GtkWidget* window, GdkEventFocus*) {
  NotifyWindowBlur();
  return FALSE;
}

// static
NativeWindow* NativeWindow::Create(content::WebContents* web_contents,
                                   base::DictionaryValue* options) {
  return new NativeWindowGtk(web_contents, options);
}

}  // namespace atom
