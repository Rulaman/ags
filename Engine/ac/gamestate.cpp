//=============================================================================
//
// Adventure Game Studio (AGS)
//
// Copyright (C) 1999-2011 Chris Jones and 2011-20xx others
// The full list of copyright holders can be found in the Copyright.txt
// file, which is part of this source code distribution.
//
// The AGS source code is provided under the Artistic License 2.0.
// A copy of this license can be found in the file License.txt and at
// http://www.opensource.org/licenses/artistic-license-2.0.php
//
//=============================================================================

#include "ac/draw.h"
#include "ac/game_version.h"
#include "ac/gamestate.h"
#include "ac/gamesetupstruct.h"
#include "ac/dynobj/scriptsystem.h"
#include "debug/debug_log.h"
#include "device/mousew32.h"
#include "game/customproperties.h"
#include "game/roomstruct.h"
#include "util/alignedstream.h"
#include "util/string_utils.h"

using namespace AGS::Common;

extern GameSetupStruct game;
extern RoomStruct thisroom;
extern CharacterInfo *playerchar;
extern ScriptSystem scsystem;

GameState::GameState()
{
    _isAutoRoomViewport = true;
    _mainViewportHasChanged = false;
    _roomViewportHasChanged = false;
    _cameraHasChanged = false;
}

const Size &GameState::GetNativeSize() const
{
    return _nativeSize;
}

void GameState::SetNativeSize(const Size &size)
{
    _nativeSize = size;
}

bool GameState::IsAutoRoomViewport() const
{
    return _isAutoRoomViewport;
}

void GameState::SetAutoRoomViewport(bool on)
{
    _isAutoRoomViewport = on;
}

Rect FixupViewport(const Rect &viewport, const Rect &parent)
{
    Size real_size = viewport.GetSize().IsNull() ? Size(1, 1) : viewport.GetSize();
    return ClampToRect(parent, RectWH(viewport.Left, viewport.Top, real_size.Width, real_size.Height));
}

void GameState::SetMainViewport(const Rect &viewport)
{
    _mainViewport.Position = FixupViewport(viewport, RectWH(game.size));
    Mouse::SetGraphicArea();
    scsystem.viewport_width = _mainViewport.Position.GetWidth();
    scsystem.viewport_height = _mainViewport.Position.GetHeight();
    _mainViewportHasChanged = true;
    // Update sub-viewports in case main viewport became smaller
    SetUIViewport(_uiViewport.Position);
    SetRoomViewport(_roomViewport.Position);
}

const Rect &GameState::GetMainViewport() const
{
    return _mainViewport.Position;
}

const Rect &GameState::GetUIViewport() const
{
    return _uiViewport.Position;
}

const Rect &GameState::GetRoomViewport() const
{
    return _roomViewport.Position;
}

Rect GameState::GetUIViewportAbs() const
{
    return Rect::MoveBy(_uiViewport.Position, _mainViewport.Position.Left, _mainViewport.Position.Top);
}

Rect GameState::GetRoomViewportAbs() const
{
    return Rect::MoveBy(_roomViewport.Position, _mainViewport.Position.Left, _mainViewport.Position.Top);
}

void GameState::SetUIViewport(const Rect &viewport)
{
    _uiViewport.Position = FixupViewport(viewport, RectWH(_mainViewport.Position.GetSize()));
}

void GameState::SetRoomViewport(const Rect &viewport)
{
    _roomViewport.Position = FixupViewport(viewport, RectWH(_mainViewport.Position.GetSize()));
    AdjustRoomToViewport();
    _roomViewportHasChanged = true;
}

void GameState::UpdateViewports()
{
    if (_mainViewportHasChanged)
        on_mainviewport_changed();
    if (_roomViewportHasChanged)
        on_roomviewport_changed();
    if (_cameraHasChanged)
        on_camera_size_changed();
    _mainViewportHasChanged = false;
    _roomViewportHasChanged = false;
    _cameraHasChanged = false;
}

const Rect &GameState::GetRoomCamera() const
{
    return _roomCamera.Position;
}

const RoomCamera &GameState::GetRoomCameraObj() const
{
    return _roomCamera;
}

void GameState::SetRoomCameraSize(const Size &cam_size)
{
    // TODO: currently we don't support having camera larger than room background
    // (or rather - looking outside of the room background); look into this later
    const Size real_room_sz = Size(thisroom.Width, thisroom.Height);
    Size real_size = Size::Clamp(cam_size, Size(1, 1), real_room_sz);

    _roomCamera.Position.SetWidth(real_size.Width);
    _roomCamera.Position.SetHeight(real_size.Height);
    AdjustRoomToViewport();
    _cameraHasChanged = true;
}

void GameState::SetRoomCameraAt(int x, int y)
{
    int cw = _roomCamera.Position.GetWidth();
    int ch = _roomCamera.Position.GetHeight();
    int room_width = thisroom.Width;
    int room_height = thisroom.Height;
    x = Math::Clamp(x, 0, room_width - cw);
    y = Math::Clamp(y, 0, room_height - ch);
    _roomCamera.Position.MoveTo(Point(x, y));
}

bool GameState::IsRoomCameraLocked() const
{
    return _roomCamera.Locked;
}

void GameState::LockRoomCamera()
{
    debug_script_log("Room camera locked");
    _roomCamera.Locked = true;
}

void GameState::LockRoomCameraAt(int x, int y)
{
    debug_script_log("Room camera locked to %d,%d", x, y);
    SetRoomCameraAt(x, y);
    _roomCamera.Locked = true;
}

void GameState::ReleaseRoomCamera()
{
    _roomCamera.Locked = false;
    debug_script_log("Room camera released back to engine control");
}

void GameState::UpdateRoomCamera()
{
    const Rect &camera = _roomCamera.Position;
    const Size real_room_sz = Size(thisroom.Width, thisroom.Height);
    if ((real_room_sz.Width > camera.GetWidth()) || (real_room_sz.Height > camera.GetHeight()))
    {
        // TODO: split out into Camera Behavior
        if (!play.IsRoomCameraLocked())
        {
            int x = playerchar->x - camera.GetWidth() / 2;
            int y = playerchar->y - camera.GetHeight() / 2;
            SetRoomCameraAt(x, y);
        }
    }
    else
    {
        SetRoomCameraAt(0, 0);
    }
}

void GameState::AdjustRoomToViewport()
{
    _roomViewport.Transform.Init(_roomCamera.Position.GetSize(), _roomViewport.Position);
}

Point GameState::RoomToScreen(int roomx, int roomy)
{
    return _roomViewport.Transform.Scale(Point(roomx - _roomCamera.Position.Left, roomy - _roomCamera.Position.Top));
}

int GameState::RoomToScreenX(int roomx)
{
    return _roomViewport.Transform.X.ScalePt(roomx - _roomCamera.Position.Left);
}

int GameState::RoomToScreenY(int roomy)
{
    return _roomViewport.Transform.Y.ScalePt(roomy - _roomCamera.Position.Top);
}

VpPoint GameState::ScreenToRoom(int scrx, int scry, bool clip_viewport)
{
    clip_viewport &= game.options[OPT_BASESCRIPTAPI] >= kScriptAPI_v3507;
    Point screen_pt(scrx, scry);
    if (clip_viewport && !_roomViewport.Position.IsInside(screen_pt))
        return std::make_pair(Point(), -1);
    Point p = _roomViewport.Transform.UnScale(screen_pt);
    p.X += _roomCamera.Position.Left;
    p.Y += _roomCamera.Position.Top;
    return std::make_pair(p, 0);
}

void GameState::ReadFromSavegame(Common::Stream *in, GameStateSvgVersion svg_ver)
{
    const bool old_save = svg_ver < kGSSvgVersion_Initial;
    score = in->ReadInt32();
    usedmode = in->ReadInt32();
    disabled_user_interface = in->ReadInt32();
    gscript_timer = in->ReadInt32();
    debug_mode = in->ReadInt32();
    in->ReadArrayOfInt32(globalvars, MAXGLOBALVARS);
    messagetime = in->ReadInt32();
    usedinv = in->ReadInt32();
    inv_top = in->ReadInt32();
    inv_numdisp = in->ReadInt32();
    obsolete_inv_numorder = in->ReadInt32();
    inv_numinline = in->ReadInt32();
    text_speed = in->ReadInt32();
    sierra_inv_color = in->ReadInt32();
    talkanim_speed = in->ReadInt32();
    inv_item_wid = in->ReadInt32();
    inv_item_hit = in->ReadInt32();
    speech_text_shadow = in->ReadInt32();
    swap_portrait_side = in->ReadInt32();
    speech_textwindow_gui = in->ReadInt32();
    follow_change_room_timer = in->ReadInt32();
    totalscore = in->ReadInt32();
    skip_display = in->ReadInt32();
    no_multiloop_repeat = in->ReadInt32();
    roomscript_finished = in->ReadInt32();
    used_inv_on = in->ReadInt32();
    no_textbg_when_voice = in->ReadInt32();
    max_dialogoption_width = in->ReadInt32();
    no_hicolor_fadein = in->ReadInt32();
    bgspeech_game_speed = in->ReadInt32();
    bgspeech_stay_on_display = in->ReadInt32();
    unfactor_speech_from_textlength = in->ReadInt32();
    mp3_loop_before_end = in->ReadInt32();
    speech_music_drop = in->ReadInt32();
    in_cutscene = in->ReadInt32();
    fast_forward = in->ReadInt32();
    room_width = in->ReadInt32();
    room_height = in->ReadInt32();
    game_speed_modifier = in->ReadInt32();
    score_sound = in->ReadInt32();
    takeover_data = in->ReadInt32();
    replay_hotkey = in->ReadInt32();
    dialog_options_x = in->ReadInt32();
    dialog_options_y = in->ReadInt32();
    narrator_speech = in->ReadInt32();
    ambient_sounds_persist = in->ReadInt32();
    lipsync_speed = in->ReadInt32();
    close_mouth_speech_time = in->ReadInt32();
    disable_antialiasing = in->ReadInt32();
    text_speed_modifier = in->ReadInt32();
    if (svg_ver < kGSSvgVersion_350)
        text_align = ConvertLegacyScriptAlignment((LegacyScriptAlignment)in->ReadInt32());
    else
        text_align = (HorAlignment)in->ReadInt32();
    speech_bubble_width = in->ReadInt32();
    min_dialogoption_width = in->ReadInt32();
    disable_dialog_parser = in->ReadInt32();
    anim_background_speed = in->ReadInt32();  // the setting for this room
    top_bar_backcolor= in->ReadInt32();
    top_bar_textcolor = in->ReadInt32();
    top_bar_bordercolor = in->ReadInt32();
    top_bar_borderwidth = in->ReadInt32();
    top_bar_ypos = in->ReadInt32();
    screenshot_width = in->ReadInt32();
    screenshot_height = in->ReadInt32();
    top_bar_font = in->ReadInt32();
    if (svg_ver < kGSSvgVersion_350)
        speech_text_align = ConvertLegacyScriptAlignment((LegacyScriptAlignment)in->ReadInt32());
    else
        speech_text_align = (HorAlignment)in->ReadInt32();
    auto_use_walkto_points = in->ReadInt32();
    inventory_greys_out = in->ReadInt32();
    skip_speech_specific_key = in->ReadInt32();
    abort_key = in->ReadInt32();
    fade_to_red = in->ReadInt32();
    fade_to_green = in->ReadInt32();
    fade_to_blue = in->ReadInt32();
    show_single_dialog_option = in->ReadInt32();
    keep_screen_during_instant_transition = in->ReadInt32();
    read_dialog_option_colour = in->ReadInt32();
    stop_dialog_at_end = in->ReadInt32();
    speech_portrait_placement = in->ReadInt32();
    speech_portrait_x = in->ReadInt32();
    speech_portrait_y = in->ReadInt32();
    speech_display_post_time_ms = in->ReadInt32();
    dialog_options_highlight_color = in->ReadInt32();
    if (old_save)
        in->ReadArrayOfInt32(reserved, GAME_STATE_RESERVED_INTS);
    // ** up to here is referenced in the script "game." object
    if (old_save)
    {
        in->ReadInt32(); // recording
        in->ReadInt32(); // playback
        in->ReadInt16(); // gamestep
    }
    randseed = in->ReadInt32();    // random seed
    player_on_region = in->ReadInt32();    // player's current region
    if (old_save)
        in->ReadInt32(); // screen_is_faded_out
    check_interaction_only = in->ReadInt32();
    bg_frame = in->ReadInt32();
    bg_anim_delay = in->ReadInt32();  // for animating backgrounds
    music_vol_was = in->ReadInt32();  // before the volume drop
    wait_counter = in->ReadInt16();
    mboundx1 = in->ReadInt16();
    mboundx2 = in->ReadInt16();
    mboundy1 = in->ReadInt16();
    mboundy2 = in->ReadInt16();
    fade_effect = in->ReadInt32();
    bg_frame_locked = in->ReadInt32();
    in->ReadArrayOfInt32(globalscriptvars, MAXGSVALUES);
    cur_music_number = in->ReadInt32();
    music_repeat = in->ReadInt32();
    music_master_volume = in->ReadInt32();
    digital_master_volume = in->ReadInt32();
    in->Read(walkable_areas_on, MAX_WALK_AREAS+1);
    screen_flipped = in->ReadInt16();
    short offsets_locked = in->ReadInt16();
    if (offsets_locked != 0)
        LockRoomCamera();
    else
        ReleaseRoomCamera();
    entered_at_x = in->ReadInt32();
    entered_at_y = in->ReadInt32();
    entered_edge = in->ReadInt32();
    want_speech = in->ReadInt32();
    cant_skip_speech = in->ReadInt32();
    in->ReadArrayOfInt32(script_timers, MAX_TIMERS);
    sound_volume = in->ReadInt32();
    speech_volume = in->ReadInt32();
    normal_font = in->ReadInt32();
    speech_font = in->ReadInt32();
    key_skip_wait = in->ReadInt8();
    swap_portrait_lastchar = in->ReadInt32();
    separate_music_lib = in->ReadInt32();
    in_conversation = in->ReadInt32();
    screen_tint = in->ReadInt32();
    num_parsed_words = in->ReadInt32();
    in->ReadArrayOfInt16( parsed_words, MAX_PARSED_WORDS);
    in->Read( bad_parsed_word, 100);
    raw_color = in->ReadInt32();
    if (old_save)
        in->ReadArrayOfInt32(raw_modified, MAX_ROOM_BGFRAMES);
    in->ReadArrayOfInt16( filenumbers, MAXSAVEGAMES);
    if (old_save)
        in->ReadInt32(); // room_changes
    mouse_cursor_hidden = in->ReadInt32();
    silent_midi = in->ReadInt32();
    silent_midi_channel = in->ReadInt32();
    current_music_repeating = in->ReadInt32();
    shakesc_delay = in->ReadInt32();
    shakesc_amount = in->ReadInt32();
    shakesc_length = in->ReadInt32();
    rtint_red = in->ReadInt32();
    rtint_green = in->ReadInt32();
    rtint_blue = in->ReadInt32();
    rtint_level = in->ReadInt32();
    rtint_light = in->ReadInt32();
    if (!old_save || loaded_game_file_version >= kGameVersion_340_4)
        rtint_enabled = in->ReadBool();
    else
        rtint_enabled = rtint_level > 0;
    end_cutscene_music = in->ReadInt32();
    skip_until_char_stops = in->ReadInt32();
    get_loc_name_last_time = in->ReadInt32();
    get_loc_name_save_cursor = in->ReadInt32();
    restore_cursor_mode_to = in->ReadInt32();
    restore_cursor_image_to = in->ReadInt32();
    music_queue_size = in->ReadInt16();
    in->ReadArrayOfInt16( music_queue, MAX_QUEUED_MUSIC);
    new_music_queue_size = in->ReadInt16();
    if (!old_save)
    {
        for (int i = 0; i < MAX_QUEUED_MUSIC; ++i)
        {
            new_music_queue[i].ReadFromFile(in);
        }
    }

    crossfading_out_channel = in->ReadInt16();
    crossfade_step = in->ReadInt16();
    crossfade_out_volume_per_step = in->ReadInt16();
    crossfade_initial_volume_out = in->ReadInt16();
    crossfading_in_channel = in->ReadInt16();
    crossfade_in_volume_per_step = in->ReadInt16();
    crossfade_final_volume_in = in->ReadInt16();

    if (old_save)
        ReadQueuedAudioItems_Aligned(in);

    in->Read(takeover_from, 50);
    in->Read(playmp3file_name, PLAYMP3FILE_MAX_FILENAME_LEN);
    in->Read(globalstrings, MAXGLOBALSTRINGS * MAX_MAXSTRLEN);
    in->Read(lastParserEntry, MAX_MAXSTRLEN);
    in->Read(game_name, 100);
    ground_level_areas_disabled = in->ReadInt32();
    next_screen_transition = in->ReadInt32();
    in->ReadInt32(); // gamma_adjustment -- do not apply gamma level from savegame
    temporarily_turned_off_character = in->ReadInt16();
    inv_backwards_compatibility = in->ReadInt16();
    if (old_save)
    {
        in->ReadInt32(); // gui_draw_order
        in->ReadInt32(); // do_once_tokens;
    }
    num_do_once_tokens = in->ReadInt32();
    if (!old_save)
    {
        do_once_tokens = new char*[num_do_once_tokens];
        for (int i = 0; i < num_do_once_tokens; ++i)
        {
            StrUtil::ReadString(&do_once_tokens[i], in);
        }
    }
    text_min_display_time_ms = in->ReadInt32();
    ignore_user_input_after_text_timeout_ms = in->ReadInt32();
    ignore_user_input_until_time = in->ReadInt32();
    if (old_save)
        in->ReadArrayOfInt32(default_audio_type_volumes, MAX_AUDIO_TYPES);
}

void GameState::WriteForSavegame(Common::Stream *out) const
{
    // NOTE: following parameters are never saved:
    // recording, playback, gamestep, screen_is_faded_out, room_changes
    out->WriteInt32(score);
    out->WriteInt32(usedmode);
    out->WriteInt32(disabled_user_interface);
    out->WriteInt32(gscript_timer);
    out->WriteInt32(debug_mode);
    out->WriteArrayOfInt32(globalvars, MAXGLOBALVARS);
    out->WriteInt32(messagetime);
    out->WriteInt32(usedinv);
    out->WriteInt32(inv_top);
    out->WriteInt32(inv_numdisp);
    out->WriteInt32(obsolete_inv_numorder);
    out->WriteInt32(inv_numinline);
    out->WriteInt32(text_speed);
    out->WriteInt32(sierra_inv_color);
    out->WriteInt32(talkanim_speed);
    out->WriteInt32(inv_item_wid);
    out->WriteInt32(inv_item_hit);
    out->WriteInt32(speech_text_shadow);
    out->WriteInt32(swap_portrait_side);
    out->WriteInt32(speech_textwindow_gui);
    out->WriteInt32(follow_change_room_timer);
    out->WriteInt32(totalscore);
    out->WriteInt32(skip_display);
    out->WriteInt32(no_multiloop_repeat);
    out->WriteInt32(roomscript_finished);
    out->WriteInt32(used_inv_on);
    out->WriteInt32(no_textbg_when_voice);
    out->WriteInt32(max_dialogoption_width);
    out->WriteInt32(no_hicolor_fadein);
    out->WriteInt32(bgspeech_game_speed);
    out->WriteInt32(bgspeech_stay_on_display);
    out->WriteInt32(unfactor_speech_from_textlength);
    out->WriteInt32(mp3_loop_before_end);
    out->WriteInt32(speech_music_drop);
    out->WriteInt32(in_cutscene);
    out->WriteInt32(fast_forward);
    out->WriteInt32(room_width);
    out->WriteInt32(room_height);
    out->WriteInt32(game_speed_modifier);
    out->WriteInt32(score_sound);
    out->WriteInt32(takeover_data);
    out->WriteInt32(replay_hotkey);
    out->WriteInt32(dialog_options_x);
    out->WriteInt32(dialog_options_y);
    out->WriteInt32(narrator_speech);
    out->WriteInt32(ambient_sounds_persist);
    out->WriteInt32(lipsync_speed);
    out->WriteInt32(close_mouth_speech_time);
    out->WriteInt32(disable_antialiasing);
    out->WriteInt32(text_speed_modifier);
    out->WriteInt32(text_align);
    out->WriteInt32(speech_bubble_width);
    out->WriteInt32(min_dialogoption_width);
    out->WriteInt32(disable_dialog_parser);
    out->WriteInt32(anim_background_speed);  // the setting for this room
    out->WriteInt32(top_bar_backcolor);
    out->WriteInt32(top_bar_textcolor);
    out->WriteInt32(top_bar_bordercolor);
    out->WriteInt32(top_bar_borderwidth);
    out->WriteInt32(top_bar_ypos);
    out->WriteInt32(screenshot_width);
    out->WriteInt32(screenshot_height);
    out->WriteInt32(top_bar_font);
    out->WriteInt32(speech_text_align);
    out->WriteInt32(auto_use_walkto_points);
    out->WriteInt32(inventory_greys_out);
    out->WriteInt32(skip_speech_specific_key);
    out->WriteInt32(abort_key);
    out->WriteInt32(fade_to_red);
    out->WriteInt32(fade_to_green);
    out->WriteInt32(fade_to_blue);
    out->WriteInt32(show_single_dialog_option);
    out->WriteInt32(keep_screen_during_instant_transition);
    out->WriteInt32(read_dialog_option_colour);
    out->WriteInt32(stop_dialog_at_end);
    out->WriteInt32(speech_portrait_placement);
    out->WriteInt32(speech_portrait_x);
    out->WriteInt32(speech_portrait_y);
    out->WriteInt32(speech_display_post_time_ms);
    out->WriteInt32(dialog_options_highlight_color);
    // ** up to here is referenced in the script "game." object
    out->WriteInt32(randseed);    // random seed
    out->WriteInt32( player_on_region);    // player's current region
    out->WriteInt32( check_interaction_only);
    out->WriteInt32( bg_frame);
    out->WriteInt32( bg_anim_delay);  // for animating backgrounds
    out->WriteInt32( music_vol_was);  // before the volume drop
    out->WriteInt16(wait_counter);
    out->WriteInt16(mboundx1);
    out->WriteInt16(mboundx2);
    out->WriteInt16(mboundy1);
    out->WriteInt16(mboundy2);
    out->WriteInt32( fade_effect);
    out->WriteInt32( bg_frame_locked);
    out->WriteArrayOfInt32(globalscriptvars, MAXGSVALUES);
    out->WriteInt32( cur_music_number);
    out->WriteInt32( music_repeat);
    out->WriteInt32( music_master_volume);
    out->WriteInt32( digital_master_volume);
    out->Write(walkable_areas_on, MAX_WALK_AREAS+1);
    out->WriteInt16( screen_flipped);
    out->WriteInt16( IsRoomCameraLocked() ? 1 : 0 );
    out->WriteInt32( entered_at_x);
    out->WriteInt32( entered_at_y);
    out->WriteInt32( entered_edge);
    out->WriteInt32( want_speech);
    out->WriteInt32( cant_skip_speech);
    out->WriteArrayOfInt32(script_timers, MAX_TIMERS);
    out->WriteInt32( sound_volume);
    out->WriteInt32( speech_volume);
    out->WriteInt32( normal_font);
    out->WriteInt32( speech_font);
    out->WriteInt8( key_skip_wait);
    out->WriteInt32( swap_portrait_lastchar);
    out->WriteInt32( separate_music_lib);
    out->WriteInt32( in_conversation);
    out->WriteInt32( screen_tint);
    out->WriteInt32( num_parsed_words);
    out->WriteArrayOfInt16( parsed_words, MAX_PARSED_WORDS);
    out->Write( bad_parsed_word, 100);
    out->WriteInt32( raw_color);
    out->WriteArrayOfInt16( filenumbers, MAXSAVEGAMES);
    out->WriteInt32( mouse_cursor_hidden);
    out->WriteInt32( silent_midi);
    out->WriteInt32( silent_midi_channel);
    out->WriteInt32( current_music_repeating);
    out->WriteInt32( shakesc_delay);
    out->WriteInt32( shakesc_amount);
    out->WriteInt32( shakesc_length);
    out->WriteInt32( rtint_red);
    out->WriteInt32( rtint_green);
    out->WriteInt32( rtint_blue);
    out->WriteInt32( rtint_level);
    out->WriteInt32( rtint_light);
    out->WriteBool(rtint_enabled);
    out->WriteInt32( end_cutscene_music);
    out->WriteInt32( skip_until_char_stops);
    out->WriteInt32( get_loc_name_last_time);
    out->WriteInt32( get_loc_name_save_cursor);
    out->WriteInt32( restore_cursor_mode_to);
    out->WriteInt32( restore_cursor_image_to);
    out->WriteInt16( music_queue_size);
    out->WriteArrayOfInt16( music_queue, MAX_QUEUED_MUSIC);
    out->WriteInt16(new_music_queue_size);
    for (int i = 0; i < MAX_QUEUED_MUSIC; ++i)
    {
        new_music_queue[i].WriteToFile(out);
    }

    out->WriteInt16( crossfading_out_channel);
    out->WriteInt16( crossfade_step);
    out->WriteInt16( crossfade_out_volume_per_step);
    out->WriteInt16( crossfade_initial_volume_out);
    out->WriteInt16( crossfading_in_channel);
    out->WriteInt16( crossfade_in_volume_per_step);
    out->WriteInt16( crossfade_final_volume_in);

    out->Write(takeover_from, 50);
    out->Write(playmp3file_name, PLAYMP3FILE_MAX_FILENAME_LEN);
    out->Write(globalstrings, MAXGLOBALSTRINGS * MAX_MAXSTRLEN);
    out->Write(lastParserEntry, MAX_MAXSTRLEN);
    out->Write(game_name, 100);
    out->WriteInt32( ground_level_areas_disabled);
    out->WriteInt32( next_screen_transition);
    out->WriteInt32( gamma_adjustment);
    out->WriteInt16(temporarily_turned_off_character);
    out->WriteInt16(inv_backwards_compatibility);
    out->WriteInt32( num_do_once_tokens);
    for (int i = 0; i < num_do_once_tokens; ++i)
    {
        StrUtil::WriteString(do_once_tokens[i], out);
    }
    out->WriteInt32( text_min_display_time_ms);
    out->WriteInt32( ignore_user_input_after_text_timeout_ms);
    out->WriteInt32( ignore_user_input_until_time);
}

void GameState::ReadQueuedAudioItems_Aligned(Common::Stream *in)
{
    AlignedStream align_s(in, Common::kAligned_Read);
    for (int i = 0; i < MAX_QUEUED_MUSIC; ++i)
    {
        new_music_queue[i].ReadFromFile(&align_s);
        align_s.Reset();
    }
}

void GameState::FreeProperties()
{
    for (int i = 0; i < game.numcharacters; ++i)
        charProps[i].clear();
    for (int i = 0; i < game.numinvitems; ++i)
        invProps[i].clear();
}

void GameState::ReadCustomProperties_v340(Common::Stream *in)
{
    if (loaded_game_file_version >= kGameVersion_340_4)
    {
        // After runtime property values were read we also copy missing default,
        // because we do not keep defaults in the saved game, and also in case
        // this save is made by an older game version which had different
        // properties.
        for (int i = 0; i < game.numcharacters; ++i)
            Properties::ReadValues(charProps[i], in);
        for (int i = 0; i < game.numinvitems; ++i)
            Properties::ReadValues(invProps[i], in);
    }
}

void GameState::WriteCustomProperties_v340(Common::Stream *out) const
{
    if (loaded_game_file_version >= kGameVersion_340_4)
    {
        // We temporarily remove properties that kept default values
        // just for the saving data time to avoid getting lots of 
        // redundant data into saved games
        for (int i = 0; i < game.numcharacters; ++i)
            Properties::WriteValues(charProps[i], out);
        for (int i = 0; i < game.numinvitems; ++i)
            Properties::WriteValues(invProps[i], out);
    }
}

// Converts legacy alignment type used in script API
HorAlignment ConvertLegacyScriptAlignment(LegacyScriptAlignment align)
{
    switch (align)
    {
    case kLegacyScAlignLeft: return kHAlignLeft;
    case kLegacyScAlignCentre: return kHAlignCenter;
    case kLegacyScAlignRight: return kHAlignRight;
    }
    return kHAlignNone;
}

// Reads legacy alignment type from the value set in script depending on the
// current Script API level. This is made to make it possible to change
// Alignment constants in the Script API and still support old version.
HorAlignment ReadScriptAlignment(int32_t align)
{
    return game.options[OPT_BASESCRIPTAPI] < kScriptAPI_v350 ?
        ConvertLegacyScriptAlignment((LegacyScriptAlignment)align) :
        (HorAlignment)align;
}
