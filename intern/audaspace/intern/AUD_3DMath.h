/*
 * $Id$
 *
 * ***** BEGIN LGPL LICENSE BLOCK *****
 *
 * Copyright 2009 Jörg Hermann Müller
 *
 * This file is part of AudaSpace.
 *
 * AudaSpace is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with AudaSpace.  If not, see <http://www.gnu.org/licenses/>.
 *
 * ***** END LGPL LICENSE BLOCK *****
 */

#ifndef AUD_3DMATH
#define AUD_3DMATH

class AUD_Quaternion
{
private:
	union
	{
		float m_v[4];
		struct
		{
			float m_w;
			float m_x;
			float m_y;
			float m_z;
		};
	};

public:
	/**
	 * Creates a new quaternion.
	 * \param w The w component.
	 * \param x The x component.
	 * \param y The y component.
	 * \param z The z component.
	 */
	inline AUD_Quaternion(float w, float x, float y, float z) :
		m_w(w), m_x(x), m_y(y), m_z(z)
	{
	}

	/**
	 * Retrieves the w component of the quarternion.
	 * \return The w component.
	 */
	inline const float& w() const
	{
		return m_w;
	}

	/**
	 * Retrieves the x component of the quarternion.
	 * \return The x component.
	 */
	inline const float& x() const
	{
		return m_x;
	}

	/**
	 * Retrieves the y component of the quarternion.
	 * \return The y component.
	 */
	inline const float& y() const
	{
		return m_y;
	}

	/**
	 * Retrieves the z component of the quarternion.
	 * \return The z component.
	 */
	inline const float& z() const
	{
		return m_z;
	}

	/**
	 * Retrieves the components of the vector.
	 * \param destination Where the 4 float values should be saved to.
	 */
	inline void get(float* destination) const
	{
		destination[0] = m_w;
		destination[1] = m_x;
		destination[2] = m_y;
		destination[3] = m_z;
	}

	/**
	 * Retrieves the components of the vector.
	 * \return The components as float[4].
	 */
	inline const float* get() const
	{
		return m_v;
	}
};

class AUD_Vector3
{
private:
	union
	{
		float m_v[3];
		struct
		{
			float m_x;
			float m_y;
			float m_z;
		};
	};

public:
	/**
	 * Creates a new 3 dimensional vector.
	 * \param x The x component.
	 * \param y The y component.
	 * \param z The z component.
	 */
	inline AUD_Vector3(float x, float y, float z) :
		m_x(x), m_y(y), m_z(z)
	{
	}

	/**
	 * Retrieves the x component of the vector.
	 * \return The x component.
	 */
	inline const float& x() const
	{
		return m_x;
	}

	/**
	 * Retrieves the y component of the vector.
	 * \return The y component.
	 */
	inline const float& y() const
	{
		return m_y;
	}

	/**
	 * Retrieves the z component of the vector.
	 * \return The z component.
	 */
	inline const float& z() const
	{
		return m_z;
	}

	/**
	 * Retrieves the components of the vector.
	 * \param destination Where the 3 float values should be saved to.
	 */
	inline void get(float* destination) const
	{
		destination[0] = m_x;
		destination[1] = m_y;
		destination[2] = m_z;
	}

	/**
	 * Retrieves the components of the vector.
	 * \return The components as float[3].
	 */
	inline const float* get() const
	{
		return m_v;
	}
};

#endif //AUD_3DMATH
