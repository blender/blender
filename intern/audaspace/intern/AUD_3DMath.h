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

/** \file audaspace/intern/AUD_3DMath.h
 *  \ingroup audaspaceintern
 */


#ifndef __AUD_3DMATH_H__
#define __AUD_3DMATH_H__

#include <cmath>
#include <cstring>

/**
 * This class represents a 3 dimensional vector.
 */
class AUD_Vector3
{
private:
	/**
	 * The vector components.
	 */
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
	inline AUD_Vector3(float x = 0, float y = 0, float z = 0) :
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
		memcpy(destination, m_v, sizeof(m_v));
	}

	/**
	 * Retrieves the components of the vector.
	 * \return The components as float[3].
	 */
	inline float* get()
	{
		return m_v;
	}

	/**
	 * Retrieves the components of the vector.
	 * \return The components as float[3].
	 */
	inline const float* get() const
	{
		return m_v;
	}

	/**
	 * Retrieves the length of the vector.
	 * \return The length of the vector.
	 */
	inline float length() const
	{
		return sqrt(m_x*m_x + m_y*m_y + m_z*m_z);
	}

	/**
	 * Retrieves the cross product.
	 * \param op The second operand.
	 * \return The cross product of the two vectors.
	 */
	inline AUD_Vector3 cross(const AUD_Vector3& op) const
	{
		return AUD_Vector3(m_y * op.m_z - m_z * op.m_y,
		                   m_z * op.m_x - m_x * op.m_z,
		                   m_x * op.m_y - m_y * op.m_x);
	}

	/**
	 * Retrieves the dot product.
	 * \param op The second operand.
	 * \return The dot product of the two vectors.
	 */
	inline float operator*(const AUD_Vector3& op) const
	{
		return m_x * op.m_x + m_y * op.m_y + m_z * op.m_z;
	}

	/**
	 * Retrieves the product with a scalar.
	 * \param op The second operand.
	 * \return The scaled vector.
	 */
	inline AUD_Vector3 operator*(const float& op) const
	{
		return AUD_Vector3(m_x * op, m_y * op, m_z * op);
	}

	/**
	 * Adds two vectors.
	 * \param op The second operand.
	 * \return The sum vector.
	 */
	inline AUD_Vector3 operator+(const AUD_Vector3& op) const
	{
		return AUD_Vector3(m_x + op.m_x, m_y + op.m_y, m_z + op.m_z);
	}

	/**
	 * Subtracts two vectors.
	 * \param op The second operand.
	 * \return The difference vector.
	 */
	inline AUD_Vector3 operator-(const AUD_Vector3& op) const
	{
		return AUD_Vector3(m_x - op.m_x, m_y - op.m_y, m_z - op.m_z);
	}

	/**
	 * Negates the vector.
	 * \return The vector facing in the opposite direction.
	 */
	inline AUD_Vector3 operator-() const
	{
		return AUD_Vector3(-m_x, -m_y, -m_z);
	}

	/**
	 * Subtracts the second vector.
	 * \param op The second operand.
	 * \return The difference vector.
	 */
	inline AUD_Vector3& operator-=(const AUD_Vector3& op)
	{
		m_x -= op.m_x;
		m_y -= op.m_y;
		m_z -= op.m_z;
		return *this;
	}
};

/**
 * This class represents a quaternion used for 3D rotations.
 */
class AUD_Quaternion
{
private:
	/**
	 * The quaternion components.
	 */
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
	inline AUD_Quaternion(float w = 1, float x = 0, float y = 0, float z = 0) :
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
		memcpy(destination, m_v, sizeof(m_v));
	}

	/**
	 * Retrieves the components of the vector.
	 * \return The components as float[4].
	 */
	inline float* get()
	{
		return m_v;
	}

	/**
	 * Retrieves the components of the vector.
	 * \return The components as float[4].
	 */
	inline const float* get() const
	{
		return m_v;
	}

	/**
	 * When the quaternion represents an orientation, this returns the negative
	 * z axis vector.
	 * \return The negative z axis vector.
	 */
	inline AUD_Vector3 getLookAt() const
	{
		return AUD_Vector3(-2 * (m_w * m_y + m_x * m_z),
							2 * (m_x * m_w - m_z * m_y),
							2 * (m_x * m_x + m_y * m_y) - 1);
	}

	/**
	 * When the quaternion represents an orientation, this returns the y axis
	 * vector.
	 * \return The y axis vector.
	 */
	inline AUD_Vector3 getUp() const
	{
		return AUD_Vector3(2 * (m_x * m_y - m_w * m_z),
							1 - 2 * (m_x * m_x + m_z * m_z),
							2 * (m_w * m_x + m_y * m_z));
	}
};

#endif //__AUD_3DMATH_H__
