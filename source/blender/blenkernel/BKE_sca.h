/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __BKE_SCA_H__
#define __BKE_SCA_H__

/** \file BKE_sca.h
 *  \ingroup bke
 */

struct Text;
struct bSensor;
struct Object;
struct bController;
struct bActuator;

void link_logicbricks(void **poin, void ***ppoin, short *tot, short size);
void unlink_logicbricks(void **poin, void ***ppoin, short *tot);

void unlink_controller(struct bController *cont);
void unlink_controllers(struct ListBase *lb);
void free_controller(struct bController *cont);
void free_controllers(struct ListBase *lb);

void unlink_actuator(struct bActuator *act);
void unlink_actuators(struct ListBase *lb);
void free_actuator(struct bActuator *act);
void free_actuators(struct ListBase *lb);

void free_sensor(struct bSensor *sens);
void free_sensors(struct ListBase *lb);
struct bSensor *copy_sensor(struct bSensor *sens);
void copy_sensors(struct ListBase *lbn, struct ListBase *lbo);
void init_sensor(struct bSensor *sens);
struct bSensor *new_sensor(int type);
struct bController *copy_controller(struct bController *cont);
void copy_controllers(struct ListBase *lbn, struct ListBase *lbo);
void init_controller(struct bController *cont);
struct bController *new_controller(int type);
struct bActuator *copy_actuator(struct bActuator *act);
void copy_actuators(struct ListBase *lbn, struct ListBase *lbo);
void init_actuator(struct bActuator *act);
struct bActuator *new_actuator(int type);
void clear_sca_new_poins_ob(struct Object *ob);
void clear_sca_new_poins(void);
void set_sca_new_poins_ob(struct Object *ob);
void set_sca_new_poins(void);
void sca_remove_ob_poin(struct Object *obt, struct Object *ob);

void sca_move_sensor(struct bSensor *sens_to_move, struct Object *ob, int move_up);
void sca_move_controller(struct bController *cont_to_move, struct Object *ob, int move_up);
void sca_move_actuator(struct bActuator *act_to_move, struct Object *ob, int move_up);

#endif

