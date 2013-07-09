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
 * Contributor(s): Tao Ju
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef QUEUE_H
#define QUEUE_H

struct gridQueueEle {
	int x, y, z;
	UCHAR dir;
	gridQueueEle *next;
};

class GridQueue
{
gridQueueEle *head;
gridQueueEle *tail;
int numEles;

public:

GridQueue( )
{
	head = NULL;
	tail = NULL;
	numEles = 0;
}

gridQueueEle *getHead( )
{
	return head;
}

int getNumElements( )
{
	return numEles;
}


void pushQueue(int st[3], int dir)
{
	gridQueueEle *ele = new gridQueueEle;
	ele->x = st[0];
	ele->y = st[1];
	ele->z = st[2];
	ele->dir = (UCHAR) dir;
	ele->next = NULL;
	if (head == NULL)
	{
		head = ele;
	}
	else {
		tail->next = ele;
	}
	tail = ele;
	numEles++;
}

int popQueue(int st[3], int& dir)
{
	if (head == NULL)
	{
		return 0;
	}

	st[0] = head->x;
	st[1] = head->y;
	st[2] = head->z;
	dir = (int) (head->dir);

	gridQueueEle *temp = head;
	head = head->next;
	delete temp;

	if (head == NULL)
	{
		tail = NULL;
	}
	numEles--;

	return 1;
}

};





#endif
