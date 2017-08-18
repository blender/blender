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
 * @file Math3D.h
 * @ingroup util
 * Defines the Vector3 and Quaternion classes.
 */

#include "Audaspace.h"

#include <cmath>
#include <cstring>

AUD_NAMESPACE_BEGIN

/**
 * This class represents a 3 dimensional vector.
 */
class AUD_API Vector3
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
	inline Vector3(float x = 0, float y = 0, float z = 0) :
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
		std::memcpy(destination, m_v, sizeof(m_v));
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
		return std::sqrt(m_x*m_x + m_y*m_y + m_z*m_z);
	}

	/**
	 * Retrieves the cross product.
	 * \param op The second operand.
	 * \return The cross product of the two vectors.
	 */
	inline Vector3 cross(const Vector3& op) const
	{
		return Vector3(m_y * op.m_z - m_z * op.m_y,
					   m_z * op.m_x - m_x * op.m_z,
					   m_x * op.m_y - m_y * op.m_x);
	}

	/**
	 * Retrieves the dot product.
	 * \param op The second operand.
	 * \return The dot product of the two vectors.
	 */
	inline float operator*(const Vector3& op) const
	{
		return m_x * op.m_x + m_y * op.m_y + m_z * op.m_z;
	}

	/**
	 * Retrieves the product with a scalar.
	 * \param op The second operand.
	 * \return The scaled vector.
	 */
	inline Vector3 operator*(const float& op) const
	{
		return Vector3(m_x * op, m_y * op, m_z * op);
	}

	/**
	 * Adds two vectors.
	 * \param op The second operand.
	 * \return The sum vector.
	 */
	inline Vector3 operator+(const Vector3& op) const
	{
		return Vector3(m_x + op.m_x, m_y + op.m_y, m_z + op.m_z);
	}

	/**
	 * Subtracts two vectors.
	 * \param op The second operand.
	 * \return The difference vector.
	 */
	inline Vector3 operator-(const Vector3& op) const
	{
		return Vector3(m_x - op.m_x, m_y - op.m_y, m_z - op.m_z);
	}

	/**
	 * Negates the vector.
	 * \return The vector facing in the opposite direction.
	 */
	inline Vector3 operator-() const
	{
		return Vector3(-m_x, -m_y, -m_z);
	}

	/**
	 * Subtracts the second vector.
	 * \param op The second operand.
	 * \return The difference vector.
	 */
	inline Vector3& operator-=(const Vector3& op)
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
class AUD_API Quaternion
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
	inline Quaternion(float w = 1, float x = 0, float y = 0, float z = 0) :
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
		std::memcpy(destination, m_v, sizeof(m_v));
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
	inline Vector3 getLookAt() const
	{
		return Vector3(-2 * (m_w * m_y + m_x * m_z),
						2 * (m_x * m_w - m_z * m_y),
						2 * (m_x * m_x + m_y * m_y) - 1);
	}

	/**
	 * When the quaternion represents an orientation, this returns the y axis
	 * vector.
	 * \return The y axis vector.
	 */
	inline Vector3 getUp() const
	{
		return Vector3(2 * (m_x * m_y - m_w * m_z),
							1 - 2 * (m_x * m_x + m_z * m_z),
							2 * (m_w * m_x + m_y * m_z));
	}
};

AUD_NAMESPACE_END
