/*******************************************************************************
 * Copyright 2009-2016 Jörg Müller
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************/

PULSEAUDIO_SYMBOL(pa_context_connect);
PULSEAUDIO_SYMBOL(pa_context_disconnect);
PULSEAUDIO_SYMBOL(pa_context_get_state);
PULSEAUDIO_SYMBOL(pa_context_new);
PULSEAUDIO_SYMBOL(pa_context_set_state_callback);
PULSEAUDIO_SYMBOL(pa_context_unref);

PULSEAUDIO_SYMBOL(pa_stream_begin_write);
PULSEAUDIO_SYMBOL(pa_stream_connect_playback);
PULSEAUDIO_SYMBOL(pa_stream_cork);
PULSEAUDIO_SYMBOL(pa_stream_flush);
PULSEAUDIO_SYMBOL(pa_stream_get_latency);
PULSEAUDIO_SYMBOL(pa_stream_is_corked);
PULSEAUDIO_SYMBOL(pa_stream_new);
PULSEAUDIO_SYMBOL(pa_stream_set_buffer_attr);
PULSEAUDIO_SYMBOL(pa_stream_set_underflow_callback);
PULSEAUDIO_SYMBOL(pa_stream_set_write_callback);
PULSEAUDIO_SYMBOL(pa_stream_write);

PULSEAUDIO_SYMBOL(pa_mainloop_free);
PULSEAUDIO_SYMBOL(pa_mainloop_get_api);
PULSEAUDIO_SYMBOL(pa_mainloop_new);
PULSEAUDIO_SYMBOL(pa_mainloop_iterate);
PULSEAUDIO_SYMBOL(pa_mainloop_prepare);
PULSEAUDIO_SYMBOL(pa_mainloop_poll);
PULSEAUDIO_SYMBOL(pa_mainloop_dispatch);

PULSEAUDIO_SYMBOL(pa_threaded_mainloop_free);
PULSEAUDIO_SYMBOL(pa_threaded_mainloop_get_api);
PULSEAUDIO_SYMBOL(pa_threaded_mainloop_lock);
PULSEAUDIO_SYMBOL(pa_threaded_mainloop_new);
PULSEAUDIO_SYMBOL(pa_threaded_mainloop_signal);
PULSEAUDIO_SYMBOL(pa_threaded_mainloop_start);
PULSEAUDIO_SYMBOL(pa_threaded_mainloop_stop);
PULSEAUDIO_SYMBOL(pa_threaded_mainloop_unlock);
PULSEAUDIO_SYMBOL(pa_threaded_mainloop_wait);
