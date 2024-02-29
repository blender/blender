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

#pragma once

/**
 * @file SoftwareDevice.h
 * @ingroup devices
 * The SoftwareDevice class.
 */

#include "devices/IDevice.h"
#include "devices/IHandle.h"
#include "devices/I3DDevice.h"
#include "devices/I3DHandle.h"
#include "devices/DefaultSynchronizer.h"
#include "util/Buffer.h"

#include <list>
#include <mutex>

AUD_NAMESPACE_BEGIN

class Mixer;
class PitchReader;
class ResampleReader;
class ChannelMapperReader;

/**
 * The software device is a generic device with software mixing.
 * It is a base class for all software mixing classes.
 * Classes implementing this have to:
 *  - Implement the playing function.
 *  - Prepare the m_specs, m_mixer variables.
 *  - Call the create and destroy functions.
 *  - Call the mix function to retrieve their audio data.
 */
class AUD_API SoftwareDevice : public IDevice, public I3DDevice
{
protected:
	/// Saves the data for playback.
	class AUD_API SoftwareHandle : public IHandle, public I3DHandle
	{
	private:
		// delete copy constructor and operator=
		SoftwareHandle(const SoftwareHandle&) = delete;
		SoftwareHandle& operator=(const SoftwareHandle&) = delete;

	public:
		/// The reader source.
		std::shared_ptr<IReader> m_reader;

		/// The pitch reader in between.
		std::shared_ptr<PitchReader> m_pitch;

		/// The resample reader in between.
		std::shared_ptr<ResampleReader> m_resampler;

		/// The channel mapper reader in between.
		std::shared_ptr<ChannelMapperReader> m_mapper;

		/// Whether the source is being read for the first time.
		bool m_first_reading;

		/// Whether to keep the source if end of it is reached.
		bool m_keep;

		/// The user set pitch of the source.
		float m_user_pitch;

		/// The user set volume of the source.
		float m_user_volume;

		/// The user set panning for non-3D sources
		float m_user_pan;

		/// The calculated final volume of the source.
		float m_volume;

		/// The previous calculated final volume of the source.
		float m_old_volume;

		/// The loop count of the source.
		int m_loopcount;

		/// Location in 3D Space.
		Vector3 m_location;

		/// Velocity in 3D Space.
		Vector3 m_velocity;

		/// Orientation in 3D Space.
		Quaternion m_orientation;

		/// Whether the position to the listener is relative or absolute
		bool m_relative;

		/// Maximum volume.
		float m_volume_max;

		/// Minimum volume.
		float m_volume_min;

		/// Maximum distance.
		float m_distance_max;

		/// Reference distance;
		float m_distance_reference;

		/// Attenuation
		float m_attenuation;

		/// Cone outer angle.
		float m_cone_angle_outer;

		/// Cone inner angle.
		float m_cone_angle_inner;

		/// Cone outer volume.
		float m_cone_volume_outer;

		/// Rendering flags
		int m_flags;

		/// The stop callback.
		stopCallback m_stop;

		/// Stop callback data.
		void* m_stop_data;

		/// Current status of the handle
		Status m_status;

		/// Own device.
		SoftwareDevice* m_device;

		/**
		 * This method is for internal use only.
		 * @param keep Whether the sound should be marked stopped or paused.
		 * @return Whether the action succeeded.
		 */
		bool pause(bool keep);

	public:
		/**
		 * Creates a new software handle.
		 * \param device The device this handle is from.
		 * \param reader The reader to play.
		 * \param pitch The pitch reader.
		 * \param resampler The resampling reader.
		 * \param mapper The channel mapping reader.
		 * \param keep Whether to keep the handle when the sound ends.
		 */
		SoftwareHandle(SoftwareDevice* device, std::shared_ptr<IReader> reader, std::shared_ptr<PitchReader> pitch, std::shared_ptr<ResampleReader> resampler, std::shared_ptr<ChannelMapperReader> mapper, bool keep);

		/**
		 * Updates the handle's playback parameters.
		 */
		void update();

		/**
		 * Sets the audio output specification of the readers.
		 * \param specs The output specification.
		 */
		void setSpecs(Specs specs);

		virtual ~SoftwareHandle() {}
		virtual bool pause();
		virtual bool resume();
		virtual bool stop();
		virtual bool getKeep();
		virtual bool setKeep(bool keep);
		virtual bool seek(double position);
		virtual double getPosition();
		virtual Status getStatus();
		virtual float getVolume();
		virtual bool setVolume(float volume);
		virtual float getPitch();
		virtual bool setPitch(float pitch);
		virtual int getLoopCount();
		virtual bool setLoopCount(int count);
		virtual bool setStopCallback(stopCallback callback = 0, void* data = 0);

		virtual Vector3 getLocation();
		virtual bool setLocation(const Vector3& location);
		virtual Vector3 getVelocity();
		virtual bool setVelocity(const Vector3& velocity);
		virtual Quaternion getOrientation();
		virtual bool setOrientation(const Quaternion& orientation);
		virtual bool isRelative();
		virtual bool setRelative(bool relative);
		virtual float getVolumeMaximum();
		virtual bool setVolumeMaximum(float volume);
		virtual float getVolumeMinimum();
		virtual bool setVolumeMinimum(float volume);
		virtual float getDistanceMaximum();
		virtual bool setDistanceMaximum(float distance);
		virtual float getDistanceReference();
		virtual bool setDistanceReference(float distance);
		virtual float getAttenuation();
		virtual bool setAttenuation(float factor);
		virtual float getConeAngleOuter();
		virtual bool setConeAngleOuter(float angle);
		virtual float getConeAngleInner();
		virtual bool setConeAngleInner(float angle);
		virtual float getConeVolumeOuter();
		virtual bool setConeVolumeOuter(float volume);
	};

	/**
	 * The specification of the device.
	 */
	DeviceSpecs m_specs;

	/**
	 * The mixer.
	 */
	std::shared_ptr<Mixer> m_mixer;

	/**
	 * Resampling quality.
	 */
	ResampleQuality m_quality;

	/**
	 * Initializes member variables.
	 */
	void create();

	/**
	 * Uninitializes member variables.
	 */
	void destroy();

	/**
	 * Mixes the next samples into the buffer.
	 * \param buffer The target buffer.
	 * \param length The length in samples to be filled.
	 */
	void mix(data_t* buffer, int length);

	/**
	 * This function tells the device, to start or pause playback.
	 * \param playing True if device should playback.
	 * \note This method is only called when the device is locked.
	 */
	virtual void playing(bool playing)=0;

	/**
	 * Sets the audio output specification of the device.
	 * \param specs The output specification.
	 */
	void setSpecs(Specs specs);

	/**
	 * Sets the audio output specification of the device.
	 * \param specs The output specification.
	 */
	void setSpecs(DeviceSpecs specs);

	/**
	 * Empty default constructor. To setup the device call the function create()
	 * and to uninitialize call destroy().
	 */
	SoftwareDevice();

private:
	/**
	 * The reading buffer.
	 */
	Buffer m_buffer;

	/**
	 * The list of sounds that are currently playing.
	 */
	std::list<std::shared_ptr<SoftwareHandle> > m_playingSounds;

	/**
	 * The list of sounds that are currently paused.
	 */
	std::list<std::shared_ptr<SoftwareHandle> > m_pausedSounds;

	/**
	 * Whether there is currently playback.
	 */
	bool m_playback;

	/**
	 * The mutex for locking.
	 */
	std::recursive_mutex m_mutex;

	/**
	 * The overall volume of the device.
	 */
	float m_volume;

	/// Listener location.
	Vector3 m_location;

	/// Listener velocity.
	Vector3 m_velocity;

	/// Listener orientation.
	Quaternion m_orientation;

	/// Speed of Sound.
	float m_speed_of_sound;

	/// Doppler factor.
	float m_doppler_factor;

	/// Distance model.
	DistanceModel m_distance_model;

	/// Rendering flags
	int m_flags;

	/// Synchronizer.
	DefaultSynchronizer m_synchronizer;

	// delete copy constructor and operator=
	SoftwareDevice(const SoftwareDevice&) = delete;
	SoftwareDevice& operator=(const SoftwareDevice&) = delete;

public:

	/**
	 * Sets the panning of a specific handle.
	 * \param handle The handle to set the panning from.
	 * \param pan The new panning value, should be in the range [-2, 2].
	 */
	static void setPanning(IHandle* handle, float pan);

	/**
	 * Sets the resampling quality.
	 * \param quality Resampling quality vs performance setting.
	 */
	void setQuality(ResampleQuality quality);

	virtual DeviceSpecs getSpecs() const;
	virtual std::shared_ptr<IHandle> play(std::shared_ptr<IReader> reader, bool keep = false);
	virtual std::shared_ptr<IHandle> play(std::shared_ptr<ISound> sound, bool keep = false);
	virtual void stopAll();
	virtual void lock();
	virtual void unlock();
	virtual float getVolume() const;
	virtual void setVolume(float volume);
	virtual ISynchronizer* getSynchronizer();

	virtual Vector3 getListenerLocation() const;
	virtual void setListenerLocation(const Vector3& location);
	virtual Vector3 getListenerVelocity() const;
	virtual void setListenerVelocity(const Vector3& velocity);
	virtual Quaternion getListenerOrientation() const;
	virtual void setListenerOrientation(const Quaternion& orientation);
	virtual float getSpeedOfSound() const;
	virtual void setSpeedOfSound(float speed);
	virtual float getDopplerFactor() const;
	virtual void setDopplerFactor(float factor);
	virtual DistanceModel getDistanceModel() const;
	virtual void setDistanceModel(DistanceModel model);
};

AUD_NAMESPACE_END
