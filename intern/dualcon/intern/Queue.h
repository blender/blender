/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef __QUEUE_H__
#define __QUEUE_H__

struct gridQueueEle {
  int x, y, z;
  UCHAR dir;
  gridQueueEle *next;
};

class GridQueue {
  gridQueueEle *head;
  gridQueueEle *tail;
  int numEles;

 public:
  GridQueue()
  {
    head = NULL;
    tail = NULL;
    numEles = 0;
  }

  gridQueueEle *getHead()
  {
    return head;
  }

  int getNumElements()
  {
    return numEles;
  }

  void pushQueue(int st[3], int dir)
  {
    gridQueueEle *ele = new gridQueueEle;
    ele->x = st[0];
    ele->y = st[1];
    ele->z = st[2];
    ele->dir = (UCHAR)dir;
    ele->next = NULL;
    if (head == NULL) {
      head = ele;
    }
    else {
      tail->next = ele;
    }
    tail = ele;
    numEles++;
  }

  int popQueue(int st[3], int &dir)
  {
    if (head == NULL) {
      return 0;
    }

    st[0] = head->x;
    st[1] = head->y;
    st[2] = head->z;
    dir = (int)(head->dir);

    gridQueueEle *temp = head;
    head = head->next;
    delete temp;

    if (head == NULL) {
      tail = NULL;
    }
    numEles--;

    return 1;
  }

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("DUALCON:GridQueue")
#endif
};

#endif /* __QUEUE_H__ */
