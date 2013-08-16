/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * Copyright 2009-2011 Jörg Hermann Müller
 *
 * This file is part of AudaSpace.
 *
 * Audaspace is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Audaspace; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file audaspace/intern/AUD_SoftwareDevice.h
 *  \ingroup audaspaceintern
 */


#ifndef __AUD_SOFTWAREDEVICE_H__
#define __AUD_SOFTWAREDEVICE_H__

#include "AUD_IDevice.h"
#include "AUD_IHandle.h"
#include "AUD_I3DDevice.h"
#include "AUD_I3DHandle.h"
#include "AUD_Mixer.h"
#include "AUD_Buffer.h"
#include "AUD_PitchReader.h"
#include "AUD_ResampleReader.h"
#include "AUD_ChannelMapperReader.h"

#include <list>
#include <pthread.h>

/**
 * This device plays is a generic device with software mixing.
 * Classes implementing this have to:
 *  - Implement the playing function.
 *  - Prepare the m_specs, m_mixer variables.
 *  - Call the create and destroy functions.
 *  - Call the mix function to retrieve their audio data.
 */
class AUD_SoftwareDevice : public AUD_IDevice, public AUD_I3DDevice
{
protected:
	/// Saves the data for playback.
	class AUD_SoftwareHandle : public AUD_IHandle, public AUD_I3DHandle
	{
	public:
		/// The reader source.
		boost::shared_ptr<AUD_IReader> m_reader;

		/// The pitch reader in between.
		boost::shared_ptr<AUD_PitchReader> m_pitch;

		/// The resample reader in between.
		boost::shared_ptr<AUD_ResampleReader> m_resampler;

		/// The channel mapper reader in between.
		boost::shared_ptr<AUD_ChannelMapperReader> m_mapper;

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

		/// The loop count of the source.
		int m_loopcount;

		/// Location in 3D Space.
		AUD_Vector3 m_location;

		/// Velocity in 3D Space.
		AUD_Vector3 m_velocity;

		/// Orientation in 3D Space.
		AUD_Quaternion m_orientation;

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
		AUD_Status m_status;

		/// Own device.
		AUD_SoftwareDevice* m_device;

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
		AUD_SoftwareHandle(AUD_SoftwareDevice* device, boost::shared_ptr<AUD_IReader> reader, boost::shared_ptr<AUD_PitchReader> pitch, boost::shared_ptr<AUD_ResampleReader> resampler, boost::shared_ptr<AUD_ChannelMapperReader> mapper, bool keep);

		/**
		 * Updates the handle's playback parameters.
		 */
		void update();

		/**
		 * Sets the audio output specification of the readers.
		 * \param sepcs The output specification.
		 */
		void setSpecs(AUD_Specs specs);

		virtual ~AUD_SoftwareHandle() {}
		virtual bool pause();
		virtual bool resume();
		virtual bool stop();
		virtual bool getKeep();
		virtual bool setKeep(bool keep);
		virtual bool seek(float position);
		virtual float getPosition();
		virtual AUD_Status getStatus();
		virtual float getVolume();
		virtual bool setVolume(float volume);
		virtual float getPitch();
		virtual bool setPitch(float pitch);
		virtual int getLoopCount();
		virtual bool setLoopCount(int count);
		virtual bool setStopCallback(stopCallback callback = 0, void* data = 0);

		virtual AUD_Vector3 getSourceLocation();
		virtual bool setSourceLocation(const AUD_Vector3& location);
		virtual AUD_Vector3 getSourceVelocity();
		virtual bool setSourceVelocity(const AUD_Vector3& velocity);
		virtual AUD_Quaternion getSourceOrientation();
		virtual bool setSourceOrientation(const AUD_Quaternion& orientation);
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

	typedef std::list<boost::shared_ptr<AUD_SoftwareHandle> >::iterator AUD_HandleIterator;

	/**
	 * The specification of the device.
	 */
	AUD_DeviceSpecs m_specs;

	/**
	 * The mixer.
	 */
	boost::shared_ptr<AUD_Mixer> m_mixer;

	/**
	 * Whether to do high or low quality resampling.
	 */
	bool m_quality;

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
	 */
	virtual void playing(bool playing)=0;

	/**
	 * Sets the audio output specification of the device.
	 * \param sepcs The output specification.
	 */
	void setSpecs(AUD_Specs specs);

private:
	/**
	 * The reading buffer.
	 */
	AUD_Buffer m_buffer;

	/**
	 * The list of sounds that are currently playing.
	 */
	std::list<boost::shared_ptr<AUD_SoftwareHandle> > m_playingSounds;

	/**
	 * The list of sounds that are currently paused.
	 */
	std::list<boost::shared_ptr<AUD_SoftwareHandle> > m_pausedSounds;

	/**
	 * Whether there is currently playback.
	 */
	bool m_playback;

	/**
	 * The mutex for locking.
	 */
	pthread_mutex_t m_mutex;

	/**
	 * The overall volume of the device.
	 */
	float m_volume;

	/// Listener location.
	AUD_Vector3 m_location;

	/// Listener velocity.
	AUD_Vector3 m_velocity;

	/// Listener orientation.
	AUD_Quaternion m_orientation;

	/// Speed of Sound.
	float m_speed_of_sound;

	/// Doppler factor.
	float m_doppler_factor;

	/// Distance model.
	AUD_DistanceModel m_distance_model;

	/// Rendering flags
	int m_flags;

public:

	/**
	 * Sets the panning of a specific handle.
	 * \param handle The handle to set the panning from.
	 * \param pan The new panning value, should be in the range [-2, 2].
	 */
	static void setPanning(AUD_IHandle* handle, float pan);

	/**
	 * Sets the resampling quality.
	 * \param quality Low (false) or high (true) quality.
	 */
	void setQuality(bool quality);

	virtual AUD_DeviceSpecs getSpecs() const;
	virtual boost::shared_ptr<AUD_IHandle> play(boost::shared_ptr<AUD_IReader> reader, bool keep = false);
	virtual boost::shared_ptr<AUD_IHandle> play(boost::shared_ptr<AUD_IFactory> factory, bool keep = false);
	virtual void stopAll();
	virtual void lock();
	virtual void unlock();
	virtual float getVolume() const;
	virtual void setVolume(float volume);

	virtual AUD_Vector3 getListenerLocation() const;
	virtual void setListenerLocation(const AUD_Vector3& location);
	virtual AUD_Vector3 getListenerVelocity() const;
	virtual void setListenerVelocity(const AUD_Vector3& velocity);
	virtual AUD_Quaternion getListenerOrientation() const;
	virtual void setListenerOrientation(const AUD_Quaternion& orientation);
	virtual float getSpeedOfSound() const;
	virtual void setSpeedOfSound(float speed);
	virtual float getDopplerFactor() const;
	virtual void setDopplerFactor(float factor);
	virtual AUD_DistanceModel getDistanceModel() const;
	virtual void setDistanceModel(AUD_DistanceModel model);
};

#endif //__AUD_SOFTWAREDEVICE_H__
