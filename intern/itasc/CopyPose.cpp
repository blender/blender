/* $Id$
 * CopyPose.cpp
 *
 *  Created on: Mar 17, 2009
 *      Author: benoit bolsee
 */

#include "CopyPose.hpp"
#include "kdl/kinfam_io.hpp"
#include <math.h>
#include <malloc.h>
#include <string.h>

namespace iTaSC
{

const unsigned int maxPoseCacheSize = (2*(3+3*2));
CopyPose::CopyPose(unsigned int control_output, unsigned int dynamic_output, double armlength, double accuracy, unsigned int maximum_iterations):
    ConstraintSet(),
	m_cache(NULL),
    m_poseCCh(-1),m_poseCTs(0)
{
	m_maxerror = armlength/2.0;
	m_outputControl = (control_output & CTL_ALL);
	int _nc = nBitsOn(m_outputControl);
	if (!_nc) 
		return;
	// reset the constraint set
	reset(_nc, accuracy, maximum_iterations);
	_nc = 0;
	m_nvalues = 0;
	int nrot = 0, npos = 0;
	int nposCache = 0, nrotCache = 0;
	m_outputDynamic = (dynamic_output & m_outputControl);
	memset(m_values, 0, sizeof(m_values));
	memset(m_posData, 0, sizeof(m_posData));
	memset(m_rotData, 0, sizeof(m_rotData));
	memset(&m_rot, 0, sizeof(m_rot));
	memset(&m_pos, 0, sizeof(m_pos));
	if (m_outputControl & CTL_POSITION) {
		m_pos.alpha = 1.0;		
		m_pos.K = 20.0;		
		m_pos.tolerance = 0.05;	
		m_values[m_nvalues].alpha = m_pos.alpha;
		m_values[m_nvalues].feedback = m_pos.K;
		m_values[m_nvalues].tolerance = m_pos.tolerance;
		m_values[m_nvalues].id = ID_POSITION;
		if (m_outputControl & CTL_POSITIONX) {
		    m_Wy(_nc) = m_pos.alpha/*/(m_pos.tolerance*m_pos.K)*/;
			m_Cf(_nc++,0)=1.0;
			m_posData[npos++].id = ID_POSITIONX;
			if (m_outputDynamic & CTL_POSITIONX)
				nposCache++;
		} 
		if (m_outputControl & CTL_POSITIONY) {
		    m_Wy(_nc) = m_pos.alpha/*/(m_pos.tolerance*m_pos.K)*/;
			m_Cf(_nc++,1)=1.0;
			m_posData[npos++].id = ID_POSITIONY;
			if (m_outputDynamic & CTL_POSITIONY)
				nposCache++;
		}
		if (m_outputControl & CTL_POSITIONZ) {
		    m_Wy(_nc) = m_pos.alpha/*/(m_pos.tolerance*m_pos.K)*/;
			m_Cf(_nc++,2)=1.0;
			m_posData[npos++].id = ID_POSITIONZ;
			if (m_outputDynamic & CTL_POSITIONZ)
				nposCache++;
		}
		m_values[m_nvalues].number = npos;
		m_values[m_nvalues++].values = m_posData;
		m_pos.firsty = 0;
		m_pos.ny = npos;
	}
	if (m_outputControl & CTL_ROTATION) {
		m_rot.alpha = 1.0;		
		m_rot.K = 20.0;		
		m_rot.tolerance = 0.05;	
		m_values[m_nvalues].alpha = m_rot.alpha;
		m_values[m_nvalues].feedback = m_rot.K;
		m_values[m_nvalues].tolerance = m_rot.tolerance;
		m_values[m_nvalues].id = ID_ROTATION;
		if (m_outputControl & CTL_ROTATIONX) {
		    m_Wy(_nc) = m_rot.alpha/*/(m_rot.tolerance*m_rot.K)*/;
			m_Cf(_nc++,3)=1.0;
			m_rotData[nrot++].id = ID_ROTATIONX;
			if (m_outputDynamic & CTL_ROTATIONX)
				nrotCache++;
		}
		if (m_outputControl & CTL_ROTATIONY) {
		    m_Wy(_nc) = m_rot.alpha/*/(m_rot.tolerance*m_rot.K)*/;
			m_Cf(_nc++,4)=1.0;
			m_rotData[nrot++].id = ID_ROTATIONY;
			if (m_outputDynamic & CTL_ROTATIONY)
				nrotCache++;
		}
		if (m_outputControl & CTL_ROTATIONZ) {
		    m_Wy(_nc) = m_rot.alpha/*/(m_rot.tolerance*m_rot.K)*/;
			m_Cf(_nc++,5)=1.0;
			m_rotData[nrot++].id = ID_ROTATIONZ;
			if (m_outputDynamic & CTL_ROTATIONZ)
				nrotCache++;
		}
		m_values[m_nvalues].number = nrot;
		m_values[m_nvalues++].values = m_rotData;
		m_rot.firsty = npos;
		m_rot.ny = nrot;
	}
	assert(_nc == m_nc);
    m_Jf=e_identity_matrix(6,6);
	m_poseCacheSize = ((nrotCache)?(3+nrotCache*2):0)+((nposCache)?(3+nposCache*2):0);
}

CopyPose::~CopyPose()
{
}

bool CopyPose::initialise(Frame& init_pose)
{
    m_externalPose = m_internalPose = init_pose;
	updateJacobian();
    return true;
}

void CopyPose::modelUpdate(Frame& _external_pose,const Timestamp& timestamp)
{
	m_internalPose = m_externalPose = _external_pose;
	updateJacobian();
}

void CopyPose::initCache(Cache *_cache)
{
    m_cache = _cache;
    m_poseCCh = -1;
    if (m_cache) {
        // create one channel for the coordinates
        m_poseCCh = m_cache->addChannel(this, "Xf", m_poseCacheSize*sizeof(double));
        // don't save initial value, it will be recomputed from external pose
        //pushPose(0);
    }
}

double* CopyPose::pushValues(double* item, ControlState* _state, unsigned int mask)
{
	ControlState::ControlValue* _yval;
	int i;

	*item++ = _state->alpha;
	*item++ = _state->K;
	*item++ = _state->tolerance;

	for (i=0, _yval=_state->output; i<_state->ny; mask<<=1) {
		if (m_outputControl & mask) {
			if (m_outputDynamic & mask) {
				*item++ = _yval->yd;
				*item++ = _yval->yddot;
			}
			_yval++;
			i++;
		}
	}
	return item;
}

void CopyPose::pushPose(CacheTS timestamp)
{
    if (m_poseCCh >= 0) {
		if (m_poseCacheSize) {
			double buf[maxPoseCacheSize];
			double *item = buf;
			if (m_outputDynamic & CTL_POSITION)
				item = pushValues(item, &m_pos, CTL_POSITIONX);
			if (m_outputDynamic & CTL_ROTATION)
				item = pushValues(item, &m_rot, CTL_ROTATIONX);
			m_cache->addCacheVectorIfDifferent(this, m_poseCCh, timestamp, buf, m_poseCacheSize, KDL::epsilon);
		} else
			m_cache->addCacheVectorIfDifferent(this, m_poseCCh, timestamp, NULL, 0, KDL::epsilon);
		m_poseCTs = timestamp;
    }
}

double* CopyPose::restoreValues(double* item, ConstraintValues* _values, ControlState* _state, unsigned int mask)
{
	ConstraintSingleValue* _data;
	ControlState::ControlValue* _yval;
	int i, j;

	_values->alpha = _state->alpha = *item++;
	_values->feedback = _state->K = *item++;
	_values->tolerance = _state->tolerance = *item++;

	for (i=_state->firsty, j=i+_state->ny, _yval=_state->output, _data=_values->values; i<j; mask<<=1) {
		if (m_outputControl & mask) {
			m_Wy(i) = _state->alpha/*/(_state->tolerance*_state->K)*/;
			if (m_outputDynamic & mask) {
				_data->yd = _yval->yd = *item++;
				_data->yddot = _yval->yddot = *item++;
			}
			_data++;
			_yval++;
			i++;
		}
	}
	return item;
}

bool CopyPose::popPose(CacheTS timestamp)
{
	bool found = false;
    if (m_poseCCh >= 0) {
        double *item = (double*)m_cache->getPreviousCacheItem(this, m_poseCCh, &timestamp);
		if (item) {
			found = true;
			if (timestamp != m_poseCTs) {
				int i=0;
				if (m_outputControl & CTL_POSITION) {
					if (m_outputDynamic & CTL_POSITION) {
						item = restoreValues(item, &m_values[i], &m_pos, CTL_POSITIONX);
					}
					i++;
				}
				if (m_outputControl & CTL_ROTATION) {
					if (m_outputDynamic & CTL_ROTATION) {
						item = restoreValues(item, &m_values[i], &m_rot, CTL_ROTATIONX);
					}
					i++;
				}
				m_poseCTs = timestamp;
				item = NULL;
			}
        }
    }
    return found;
}

void CopyPose::interpolateOutput(ControlState* _state, unsigned int mask, const Timestamp& timestamp)
{
	ControlState::ControlValue* _yval;
	int i;

	for (i=0, _yval=_state->output; i<_state->ny; mask <<= 1) {
		if (m_outputControl & mask) {
			if (m_outputDynamic & mask) {
				if (timestamp.substep && timestamp.interpolate) {
					_yval->yd += _yval->yddot*timestamp.realTimestep;
				} else {
					_yval->yd = _yval->nextyd;
					_yval->yddot = _yval->nextyddot;
				}
			}
			i++;
			_yval++;
		}
	}
}

void CopyPose::pushCache(const Timestamp& timestamp)
{
	if (!timestamp.substep && timestamp.cache) {
        pushPose(timestamp.cacheTimestamp);
	}
}

void CopyPose::updateKinematics(const Timestamp& timestamp)
{
	if (timestamp.interpolate) {
		if (m_outputDynamic & CTL_POSITION)
			interpolateOutput(&m_pos, CTL_POSITIONX, timestamp);
		if (m_outputDynamic & CTL_ROTATION)
			interpolateOutput(&m_rot, CTL_ROTATIONX, timestamp);
	}
	pushCache(timestamp);
}

void CopyPose::updateJacobian()
{
    //Jacobian is always identity at the start of the constraint chain
	//instead of going through complicated jacobian operation, implemented direct formula
	//m_Jf(1,3) = m_internalPose.p.z();
	//m_Jf(2,3) = -m_internalPose.p.y();
	//m_Jf(0,4) = -m_internalPose.p.z();
	//m_Jf(2,4) = m_internalPose.p.x();
	//m_Jf(0,5) = m_internalPose.p.y();
	//m_Jf(1,5) = -m_internalPose.p.x();
}

void CopyPose::updateState(ConstraintValues* _values, ControlState* _state, unsigned int mask, double timestep)
{
	int id = (mask == CTL_ROTATIONX) ? ID_ROTATIONX : ID_POSITIONX;
	ControlState::ControlValue* _yval;
	ConstraintSingleValue* _data;
	int i, j, k;
    int action = 0;

    if ((_values->action & ACT_ALPHA) && _values->alpha >= 0.0) {
        _state->alpha = _values->alpha;
        action |= ACT_ALPHA;
    }
    if ((_values->action & ACT_TOLERANCE) && _values->tolerance > KDL::epsilon) {
        _state->tolerance = _values->tolerance;
        action |= ACT_TOLERANCE;
    }
    if ((_values->action & ACT_FEEDBACK) && _values->feedback > KDL::epsilon) {
        _state->K = _values->feedback;
        action |= ACT_FEEDBACK;
    }
	for (i=_state->firsty, j=_state->firsty+_state->ny, _yval=_state->output; i<j; mask <<= 1, id++) {
		if (m_outputControl & mask) {
			if (action)
				m_Wy(i) = _state->alpha/*/(_state->tolerance*_state->K)*/;
			// check if this controlled output is provided
			for (k=0, _data=_values->values; k<_values->number; k++, _data++) {
				if (_data->id == id) {
					switch (_data->action & (ACT_VALUE|ACT_VELOCITY)) {
					case 0:
						// no indication, keep current values
						break;
					case ACT_VELOCITY:
						// only the velocity is given estimate the new value by integration
						_data->yd = _yval->yd+_data->yddot*timestep;
						// walkthrough
					case ACT_VALUE:
						_yval->nextyd = _data->yd;
						// if the user sets the value, we assume future velocity is zero
						// (until the user changes the value again)
						_yval->nextyddot = (_data->action & ACT_VALUE) ? 0.0 : _data->yddot;
						if (timestep>0.0) {
							_yval->yddot = (_data->yd-_yval->yd)/timestep;
						} else {
							// allow the user to change target instantenously when this function
							// if called from setControlParameter with timestep = 0
							_yval->yd = _yval->nextyd;
							_yval->yddot = _yval->nextyddot;
						}
						break;
					case (ACT_VALUE|ACT_VELOCITY):
						// the user should not set the value and velocity at the same time.
						// In this case, we will assume that he wants to set the future value
						// and we compute the current value to match the velocity
						_yval->yd = _data->yd - _data->yddot*timestep;
						_yval->nextyd = _data->yd;
						_yval->nextyddot = _data->yddot;
						if (timestep>0.0) {
							_yval->yddot = (_data->yd-_yval->yd)/timestep;
						} else {
							_yval->yd = _yval->nextyd;
							_yval->yddot = _yval->nextyddot;
						}
						break;
					}
				}
			}
			_yval++;
			i++;
		}
	}
}


bool CopyPose::setControlParameters(struct ConstraintValues* _values, unsigned int _nvalues, double timestep)
{
	while (_nvalues > 0) {
		if (_values->id >= ID_POSITION && _values->id <= ID_POSITIONZ && (m_outputControl & CTL_POSITION)) {
			updateState(_values, &m_pos, CTL_POSITIONX, timestep);
		} 
		if (_values->id >= ID_ROTATION && _values->id <= ID_ROTATIONZ && (m_outputControl & CTL_ROTATION)) {
			updateState(_values, &m_rot, CTL_ROTATIONX, timestep);
		}
		_values++;
		_nvalues--;
	}
    return true;
}

void CopyPose::updateValues(Vector& vel, ConstraintValues* _values, ControlState* _state, unsigned int mask)
{
	ConstraintSingleValue* _data;
	ControlState::ControlValue* _yval;
	int i, j;

	_values->action = 0;

	for (i=_state->firsty, j=0, _yval=_state->output, _data=_values->values; j<3; j++, mask<<=1) {
		if (m_outputControl & mask) {
			*(double*)&_data->y = vel(j);
			*(double*)&_data->ydot = m_ydot(i);
			_data->yd = _yval->yd;
			_data->yddot = _yval->yddot;
			_data->action = 0;
			i++;
			_data++;
			_yval++;
		}
	}
}

void CopyPose::updateOutput(Vector& vel, ControlState* _state, unsigned int mask)
{
	ControlState::ControlValue* _yval;
	int i, j;
	double coef=1.0;
	if (mask & CTL_POSITION) {
		// put a limit on position error
		double len=0.0;
		for (j=0, _yval=_state->output; j<3; j++) {
			if (m_outputControl & (mask<<j)) {
				len += KDL::sqr(_yval->yd-vel(j));
				_yval++;
			}
		}
		len = KDL::sqrt(len);
		if (len > m_maxerror)
			coef = m_maxerror/len;
	}
	for (i=_state->firsty, j=0, _yval=_state->output; j<3; j++) {
		if (m_outputControl & (mask<<j)) {
			m_ydot(i)=_yval->yddot+_state->K*coef*(_yval->yd-vel(j));
			_yval++;
			i++;
		}
	}
}

void CopyPose::updateControlOutput(const Timestamp& timestamp)
{
    //IMO this should be done, no idea if it is enough (wrt Distance impl)
	Twist y = diff(F_identity, m_internalPose);
	bool found = true;
	if (!timestamp.substep) {
		if (!timestamp.reiterate) {
			found = popPose(timestamp.cacheTimestamp);
		}
	}
	if (m_constraintCallback && (m_substep || (!timestamp.reiterate && !timestamp.substep))) {
		// initialize first callback the application to get the current values
		int i=0;
		if (m_outputControl & CTL_POSITION) {
			updateValues(y.vel, &m_values[i++], &m_pos, CTL_POSITIONX);
		}
		if (m_outputControl & CTL_ROTATION) {
			updateValues(y.rot, &m_values[i++], &m_rot, CTL_ROTATIONX);
		}
		if ((*m_constraintCallback)(timestamp, m_values, m_nvalues, m_constraintParam)) {
			setControlParameters(m_values, m_nvalues, (found && timestamp.interpolate)?timestamp.realTimestep:0.0);
		}
	}
	if (m_outputControl & CTL_POSITION) {
		updateOutput(y.vel, &m_pos, CTL_POSITIONX);
	}
	if (m_outputControl & CTL_ROTATION) {
		updateOutput(y.rot, &m_rot, CTL_ROTATIONX);
	}
}

const ConstraintValues* CopyPose::getControlParameters(unsigned int* _nvalues)
{
	Twist y = diff(m_internalPose,F_identity);
	int i=0;
	if (m_outputControl & CTL_POSITION) {
		updateValues(y.vel, &m_values[i++], &m_pos, CTL_POSITIONX);
	}
	if (m_outputControl & CTL_ROTATION) {
		updateValues(y.rot, &m_values[i++], &m_rot, CTL_ROTATIONX);
	}
	if (_nvalues)
		*_nvalues=m_nvalues; 
	return m_values; 
}

double CopyPose::getMaxTimestep(double& timestep)
{
	// CopyPose should not have any limit on linear velocity: 
	// in case the target is out of reach, this can be very high.
	// We will simply limit on rotation
	e_scalar maxChidot = m_chidot.block(3,0,3,1).cwise().abs().maxCoeff();
	if (timestep*maxChidot > m_maxDeltaChi) {
		timestep = m_maxDeltaChi/maxChidot;
	}
	return timestep;
}

}
