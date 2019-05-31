/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __FREESTYLE_MATERIAL_H__
#define __FREESTYLE_MATERIAL_H__

/** \file
 * \ingroup freestyle
 * \brief Class used to handle materials.
 */

#include "../system/FreestyleConfig.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

/*! Class defining a material */
class FrsMaterial {
 public:
  /*! Default constructor */
  inline FrsMaterial();

  /*! Builds a Material from its line, diffuse, ambiant, specular, emissive
   *  colors, a shininess coefficient and line color priority.
   *    \param iLine:
   *      A 4 element float-array containing the line color.
   *    \param iDiffuse:
   *      A 4 element float-array containing the diffuse color.
   *    \param iAmbiant:
   *      A 4 element float-array containing the ambiant color.
   *    \param iSpecular:
   *      A 4 element float-array containing the specular color.
   *    \param iEmission:
   *      A 4 element float-array containing the emissive color.
   *    \param iShininess:
   *      The shininess coefficient.
   *    \param iPriority:
   *      The line color priority.
   */
  inline FrsMaterial(const float *iLine,
                     const float *iDiffuse,
                     const float *iAmbiant,
                     const float *iSpecular,
                     const float *iEmission,
                     const float iShininess,
                     const int iPriority);

  /*! Copy constructor */
  inline FrsMaterial(const FrsMaterial &m);

  /*! Destructor */
  virtual ~FrsMaterial()
  {
  }

  /*! Returns the line color as a 4 float array */
  inline const float *line() const
  {
    return Line;
  }

  /*! Returns the red component of the line color */
  inline const float lineR() const
  {
    return Line[0];
  }

  /*! Returns the green component of the line color */
  inline const float lineG() const
  {
    return Line[1];
  }

  /*! Returns the blue component of the line color */
  inline const float lineB() const
  {
    return Line[2];
  }

  /*! Returns the alpha component of the line color */
  inline const float lineA() const
  {
    return Line[3];
  }

  /*! Returns the diffuse color as a 4 float array */
  inline const float *diffuse() const
  {
    return Diffuse;
  }

  /*! Returns the red component of the diffuse color */
  inline const float diffuseR() const
  {
    return Diffuse[0];
  }

  /*! Returns the green component of the diffuse color */
  inline const float diffuseG() const
  {
    return Diffuse[1];
  }

  /*! Returns the blue component of the diffuse color */
  inline const float diffuseB() const
  {
    return Diffuse[2];
  }

  /*! Returns the alpha component of the diffuse color */
  inline const float diffuseA() const
  {
    return Diffuse[3];
  }

  /*! Returns the specular color as a 4 float array */
  inline const float *specular() const
  {
    return Specular;
  }

  /*! Returns the red component of the specular color */
  inline const float specularR() const
  {
    return Specular[0];
  }

  /*! Returns the green component of the specular color */
  inline const float specularG() const
  {
    return Specular[1];
  }

  /*! Returns the blue component of the specular color */
  inline const float specularB() const
  {
    return Specular[2];
  }

  /*! Returns the alpha component of the specular color */
  inline const float specularA() const
  {
    return Specular[3];
  }

  /*! Returns the ambiant color as a 4 float array */
  inline const float *ambient() const
  {
    return Ambient;
  }

  /*! Returns the red component of the ambiant color */
  inline const float ambientR() const
  {
    return Ambient[0];
  }

  /*! Returns the green component of the ambiant color */
  inline const float ambientG() const
  {
    return Ambient[1];
  }

  /*! Returns the blue component of the ambiant color */
  inline const float ambientB() const
  {
    return Ambient[2];
  }

  /*! Returns the alpha component of the ambiant color */
  inline const float ambientA() const
  {
    return Ambient[3];
  }

  /*! Returns the emissive color as a 4 float array */
  inline const float *emission() const
  {
    return Emission;
  }

  /*! Returns the red component of the emissive color */
  inline const float emissionR() const
  {
    return Emission[0];
  }

  /*! Returns the green component of the emissive color */
  inline const float emissionG() const
  {
    return Emission[1];
  }

  /*! Returns the blue component of the emissive color */
  inline const float emissionB() const
  {
    return Emission[2];
  }

  /*! Returns the alpha component of the emissive color */
  inline const float emissionA() const
  {
    return Emission[3];
  }

  /*! Returns the shininess coefficient */
  inline const float shininess() const
  {
    return Shininess;
  }

  /*! Returns the line color priority */
  inline const int priority() const
  {
    return Priority;
  }

  /*! Sets the line color.
   *    \param r:
   *      Red component
   *    \param g:
   *      Green component
   *    \param b:
   *     Blue component
   *    \param a:
   *      Alpha component
   */
  inline void setLine(const float r, const float g, const float b, const float a);

  /*! Sets the diffuse color.
   *    \param r:
   *      Red component
   *    \param g:
   *      Green component
   *    \param b:
   *     Blue component
   *    \param a:
   *      Alpha component
   */
  inline void setDiffuse(const float r, const float g, const float b, const float a);

  /*! Sets the specular color.
   *    \param r:
   *      Red component
   *    \param g:
   *      Green component
   *    \param b:
   *     Blue component
   *    \param a:
   *      Alpha component
   */
  inline void setSpecular(const float r, const float g, const float b, const float a);

  /*! Sets the ambiant color.
   *    \param r:
   *      Red component
   *    \param g:
   *      Green component
   *    \param b:
   *     Blue component
   *    \param a:
   *      Alpha component
   */
  inline void setAmbient(const float r, const float g, const float b, const float a);

  /*! Sets the emissive color.
   *    \param r:
   *      Red component
   *    \param g:
   *      Green component
   *    \param b:
   *     Blue component
   *    \param a:
   *      Alpha component
   */
  inline void setEmission(const float r, const float g, const float b, const float a);

  /*! Sets the shininess.
   *    \param s:
   *      Shininess
   */
  inline void setShininess(const float s);

  /*! Sets the line color priority.
   *    \param priority:
   *      Priority
   */
  inline void setPriority(const int priority);

  /* operators */
  inline FrsMaterial &operator=(const FrsMaterial &m);
  inline bool operator!=(const FrsMaterial &m) const;
  inline bool operator==(const FrsMaterial &m) const;

 private:
  /*! Material properties */
  float Line[4];
  float Diffuse[4];
  float Specular[4];
  float Ambient[4];
  float Emission[4];
  float Shininess;
  int Priority;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:FrsMaterial")
#endif
};

FrsMaterial::FrsMaterial()
{
  Line[0] = Line[1] = Line[2] = 0.0f;
  Line[3] = 1.0f;

  Ambient[0] = Ambient[1] = Ambient[2] = 0.2f;
  Ambient[3] = 1.0f;

  Diffuse[0] = Diffuse[1] = Diffuse[2] = 0.8f;
  Diffuse[3] = 1.0f;

  Emission[0] = Emission[1] = Emission[2] = 0.0f;
  Emission[3] = 1.0f;

  Specular[0] = Specular[1] = Specular[2] = 0.0f;
  Specular[3] = 1.0f;

  Shininess = 0.0f;
  Priority = 0;
}

FrsMaterial::FrsMaterial(const float *iLine,
                         const float *iDiffuse,
                         const float *iAmbiant,
                         const float *iSpecular,
                         const float *iEmission,
                         const float iShininess,
                         const int iPriority)
{
  for (int i = 0; i < 4; i++) {
    Line[i] = iLine[i];
    Diffuse[i] = iDiffuse[i];
    Specular[i] = iSpecular[i];
    Ambient[i] = iAmbiant[i];
    Emission[i] = iEmission[i];
  }

  Shininess = iShininess;
  Priority = iPriority;
}

FrsMaterial::FrsMaterial(const FrsMaterial &m)
{
  for (int i = 0; i < 4; i++) {
    Line[i] = m.line()[i];
    Diffuse[i] = m.diffuse()[i];
    Specular[i] = m.specular()[i];
    Ambient[i] = m.ambient()[i];
    Emission[i] = m.emission()[i];
  }

  Shininess = m.shininess();
  Priority = m.priority();
}

void FrsMaterial::setLine(const float r, const float g, const float b, const float a)
{
  Line[0] = r;
  Line[1] = g;
  Line[2] = b;
  Line[3] = a;
}

void FrsMaterial::setDiffuse(const float r, const float g, const float b, const float a)
{
  Diffuse[0] = r;
  Diffuse[1] = g;
  Diffuse[2] = b;
  Diffuse[3] = a;
}

void FrsMaterial::setSpecular(const float r, const float g, const float b, const float a)
{
  Specular[0] = r;
  Specular[1] = g;
  Specular[2] = b;
  Specular[3] = a;
}

void FrsMaterial::setAmbient(const float r, const float g, const float b, const float a)
{
  Ambient[0] = r;
  Ambient[1] = g;
  Ambient[2] = b;
  Ambient[3] = a;
}

void FrsMaterial::setEmission(const float r, const float g, const float b, const float a)
{
  Emission[0] = r;
  Emission[1] = g;
  Emission[2] = b;
  Emission[3] = a;
}

void FrsMaterial::setShininess(const float s)
{
  Shininess = s;
}

void FrsMaterial::setPriority(const int priority)
{
  Priority = priority;
}

FrsMaterial &FrsMaterial::operator=(const FrsMaterial &m)
{
  for (int i = 0; i < 4; i++) {
    Line[i] = m.line()[i];
    Diffuse[i] = m.diffuse()[i];
    Specular[i] = m.specular()[i];
    Ambient[i] = m.ambient()[i];
    Emission[i] = m.emission()[i];
  }

  Shininess = m.shininess();
  Priority = m.priority();
  return *this;
}

bool FrsMaterial::operator!=(const FrsMaterial &m) const
{
  if (Shininess != m.shininess()) {
    return true;
  }
  if (Priority != m.priority()) {
    return true;
  }

  for (int i = 0; i < 4; i++) {
    if (Line[i] != m.line()[i]) {
      return true;
    }
    if (Diffuse[i] != m.diffuse()[i]) {
      return true;
    }
    if (Specular[i] != m.specular()[i]) {
      return true;
    }
    if (Ambient[i] != m.ambient()[i]) {
      return true;
    }
    if (Emission[i] != m.emission()[i]) {
      return true;
    }
  }

  return false;
}

bool FrsMaterial::operator==(const FrsMaterial &m) const
{
  return (!((*this) != m));
}

} /* namespace Freestyle */

#endif  // __FREESTYLE_MATERIAL_H__
