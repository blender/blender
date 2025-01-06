/*******************************************************************************
 * Copyright 2009-2024 Jörg Müller
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

PIPEWIRE_SYMBOL(pw_init);
PIPEWIRE_SYMBOL(pw_deinit);

PIPEWIRE_SYMBOL(pw_properties_new);
PIPEWIRE_SYMBOL(pw_properties_setf);

PIPEWIRE_SYMBOL(pw_stream_connect);
PIPEWIRE_SYMBOL(pw_stream_destroy);
PIPEWIRE_SYMBOL(pw_stream_get_nsec);
PIPEWIRE_SYMBOL(pw_stream_get_time_n);
PIPEWIRE_SYMBOL(pw_stream_new_simple);
PIPEWIRE_SYMBOL(pw_stream_queue_buffer);
PIPEWIRE_SYMBOL(pw_stream_dequeue_buffer);
PIPEWIRE_SYMBOL(pw_stream_set_active);
PIPEWIRE_SYMBOL(pw_stream_flush);

PIPEWIRE_SYMBOL(pw_thread_loop_destroy);
PIPEWIRE_SYMBOL(pw_thread_loop_get_loop);
PIPEWIRE_SYMBOL(pw_thread_loop_lock);
PIPEWIRE_SYMBOL(pw_thread_loop_unlock);
PIPEWIRE_SYMBOL(pw_thread_loop_new);
PIPEWIRE_SYMBOL(pw_thread_loop_start);
PIPEWIRE_SYMBOL(pw_thread_loop_stop);

PIPEWIRE_SYMBOL(pw_check_library_version);
