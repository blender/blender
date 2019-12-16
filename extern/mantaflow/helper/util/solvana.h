/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2011 Tobias Pfaff, Nils Thuerey
 *
 * This program is free software, distributed under the terms of the
 * Apache License, Version 2.0
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Analytical solutions to some problems
 * generated using MATLAB symbolic math ccode
 *
 ******************************************************************************/

#ifndef _SOLVANA_H
#define _SOLVANA_H

//! solves the equation [e1 e2 e3; 1 1 1]*x = g using least squares
inline void SolveOverconstraint34(float e1x,
                                  float e1y,
                                  float e1z,
                                  float e2x,
                                  float e2y,
                                  float e2z,
                                  float e3x,
                                  float e3y,
                                  float e3z,
                                  float g1,
                                  float g2,
                                  float g3,
                                  float &x1,
                                  float &x2,
                                  float &x3)
{
  float e1x2 = e1x * e1x, e1y2 = e1y * e1y, e1z2 = e1z * e1z;
  float e2x2 = e2x * e2x, e2y2 = e2y * e2y, e2z2 = e2z * e2z;
  float e3x2 = e3x * e3x, e3y2 = e3y * e3y, e3z2 = e3z * e3z;
  float e1xy = e1x * e1y, e1xz = e1x * e1z, e1yz = e1y * e1z;
  float e2xy = e2x * e2y, e2xz = e2x * e2z, e2yz = e2y * e2z;
  float e3xy = e3x * e3y, e3xz = e3x * e3z, e3yz = e3y * e3z;
  float e12x = e1x * e2x, e12y = e1y * e2y, e12z = e1z * e2z;
  float e13x = e1x * e3x, e13y = e1y * e3y, e13z = e1z * e3z;
  float e23x = e2x * e3x, e23y = e2y * e3y, e23z = e2z * e3z;
  float t1543 = e3y2 * e2x2;
  float t1544 = e3x2 * e2y2;
  float t1545 = e3z2 * e2x2;
  float t1546 = e3x2 * e2z2;
  float t1547 = e3z2 * e2y2;
  float t1548 = e3y2 * e2z2;
  float t1549 = e2y2 * e1x2;
  float t1550 = e2x2 * e1y2;
  float t1551 = e2z2 * e1x2;
  float t1552 = e2x2 * e1z2;
  float t1553 = e2z2 * e1y2;
  float t1554 = e2y2 * e1z2;
  float t1555 = e3y2 * e1x2;
  float t1556 = e3x2 * e1y2;
  float t1557 = e3z2 * e1x2;
  float t1558 = e3x2 * e1z2;
  float t1559 = e3z2 * e1y2;
  float t1560 = e3y2 * e1z2;
  float t1561 = e3z2 * e2y2 * e1x2;
  float t1562 = e3y2 * e2z2 * e1x2;
  float t1563 = e3z2 * e2x2 * e1y2;
  float t1564 = e3x2 * e2z2 * e1y2;
  float t1565 = e3y2 * e2x2 * e1z2;
  float t1566 = e3x2 * e2y2 * e1z2;
  float t1567 = e1xy * e2x * e3y * 2.0;
  float t1568 = e1xy * e2y * e3x * 2.0;
  float t1569 = e1xz * e2x * e3z * 2.0;
  float t1570 = e1xz * e2z * e3x * 2.0;
  float t1571 = e1yz * e2y * e3z * 2.0;
  float t1572 = e1yz * e2z * e3y * 2.0;
  float t1573 = e1x * e2xy * e3y * 2.0;
  float t1574 = e1y * e2xy * e3x * 2.0;
  float t1575 = e1x * e2xz * e3z * 2.0;
  float t1576 = e1z * e2xz * e3x * 2.0;
  float t1577 = e1y * e2yz * e3z * 2.0;
  float t1578 = e1z * e2yz * e3y * 2.0;
  float t1579 = e1x * e2y * e3xy * 2.0;
  float t1580 = e1y * e2x * e3xy * 2.0;
  float t1581 = e1x * e2z * e3xz * 2.0;
  float t1582 = e1z * e2x * e3xz * 2.0;
  float t1583 = e1y * e2z * e3yz * 2.0;
  float t1584 = e1z * e2y * e3yz * 2.0;
  float t1585 = e1xy * e2xz * e3yz * 2.0;
  float t1586 = e1xy * e2yz * e3xz * 2.0;
  float t1587 = e1xz * e2xy * e3yz * 2.0;
  float t1588 = e1xz * e2yz * e3xy * 2.0;
  float t1589 = e1yz * e2xy * e3xz * 2.0;
  float t1590 = e1yz * e2xz * e3xy * 2.0;
  float t1596 = e12x * e3y2 * 2.0;
  float t1597 = e13x * e2y2 * 2.0;
  float t1598 = e23x * e1y2 * 2.0;
  float t1599 = e12x * e3z2 * 2.0;
  float t1600 = e13x * e2z2 * 2.0;
  float t1601 = e12y * e3x2 * 2.0;
  float t1602 = e13y * e2x2 * 2.0;
  float t1603 = e23y * e1x2 * 2.0;
  float t1604 = e23x * e1z2 * 2.0;
  float t1605 = e12y * e3z2 * 2.0;
  float t1606 = e13y * e2z2 * 2.0;
  float t1607 = e12z * e3x2 * 2.0;
  float t1608 = e13z * e2x2 * 2.0;
  float t1609 = e23z * e1x2 * 2.0;
  float t1610 = e23y * e1z2 * 2.0;
  float t1611 = e12z * e3y2 * 2.0;
  float t1612 = e13z * e2y2 * 2.0;
  float t1613 = e23z * e1y2 * 2.0;
  float t1614 = e1xy * e2xy * 2.0;
  float t1615 = e1xz * e2xz * 2.0;
  float t1616 = e1yz * e2yz * 2.0;
  float t1617 = e1xy * e3xy * 2.0;
  float t1618 = e1xz * e3xz * 2.0;
  float t1619 = e1yz * e3yz * 2.0;
  float t1620 = e2xy * e3xy * 2.0;
  float t1621 = e2xz * e3xz * 2.0;
  float t1622 = e2yz * e3yz * 2.0;
  float t1623 = e1xy * e2xy * e3z2 * 2.0;
  float t1624 = e1xz * e2xz * e3y2 * 2.0;
  float t1625 = e1yz * e2yz * e3x2 * 2.0;
  float t1626 = e1xy * e3xy * e2z2 * 2.0;
  float t1627 = e1xz * e3xz * e2y2 * 2.0;
  float t1628 = e1yz * e3yz * e2x2 * 2.0;
  float t1629 = e2xy * e3xy * e1z2 * 2.0;
  float t1630 = e2xz * e3xz * e1y2 * 2.0;
  float t1631 = e2yz * e3yz * e1x2 * 2.0;
  float t1591 = t1550 + t1551 + t1560 + t1543 + t1552 + t1561 + t1570 + t1544 + t1553 + t1562 +
                t1571 + t1580 + t1545 + t1554 + t1563 + t1572 + t1581 + t1590 + t1546 + t1555 +
                t1564 + t1573 + t1582 + t1547 + t1556 + t1565 + t1574 + t1583 + t1548 + t1557 +
                t1566 + t1575 + t1584 + t1549 + t1558 + t1567 + t1576 + t1585 + t1559 + t1568 +
                t1577 + t1586 + t1569 + t1578 + t1587 - t1596 + t1579 + t1588 - t1597 + t1589 -
                t1598 - t1599 - t1600 - t1601 - t1610 - t1602 - t1611 - t1620 - t1603 - t1612 -
                t1621 - t1630 - t1604 - t1613 - t1622 - t1631 - t1605 - t1614 - t1623 - t1606 -
                t1615 - t1624 - t1607 - t1616 - t1625 - t1608 - t1617 - t1626 - t1609 - t1618 -
                t1627 - t1619 - t1628 - t1629;
  float t1592 = 1.0 / t1591;
  float t1635 = e13x * e2y2;
  float t1636 = e13x * e2z2;
  float t1637 = e13y * e2x2;
  float t1638 = e13y * e2z2;
  float t1639 = e13z * e2x2;
  float t1640 = e13z * e2y2;
  float t1653 = e23x * 2.0;
  float t1654 = e23y * 2.0;
  float t1655 = e23z * 2.0;
  float t1641 = e3x2 + e3z2 + e3y2 + e2y2 + t1543 + e2z2 + t1544 + e2x2 + t1545 + t1546 + t1547 +
                t1548 - t1620 - t1621 - t1622 - t1653 - t1654 - t1655;
  float t1642 = e12x * e3y2;
  float t1643 = e12x * e3z2;
  float t1644 = e12y * e3x2;
  float t1645 = e12y * e3z2;
  float t1646 = e12z * e3x2;
  float t1647 = e12z * e3y2;
  float t1656 = e1x * e2y * e3xy;
  float t1657 = e1y * e2x * e3xy;
  float t1658 = e1x * e2z * e3xz;
  float t1659 = e1z * e2x * e3xz;
  float t1660 = e1y * e2z * e3yz;
  float t1661 = e1z * e2y * e3yz;
  float t1648 = e3x2 + e3z2 + e3y2 - e13x - e13y - e13z + e12x - e23y + e12y + t1642 - e23z -
                t1660 + e12z + t1643 - t1661 + t1644 + t1645 + t1646 + t1647 - t1656 - t1657 -
                e23x - t1658 - t1659;
  float t1679 = e1x * e2xy * e3y;
  float t1680 = e1y * e2xy * e3x;
  float t1681 = e1x * e2xz * e3z;
  float t1682 = e1z * e2xz * e3x;
  float t1683 = e1y * e2yz * e3z;
  float t1684 = e1z * e2yz * e3y;
  float t1652 = e2y2 + e2z2 + e2x2 + e13x + e13y + e13z + t1640 - e12x - e23y - e12y - e23z -
                e12z + t1635 - t1680 + t1636 - t1681 + t1637 - t1682 + t1638 - t1683 + t1639 -
                t1684 - e23x - t1679;
  float t1662 = e23x * e1y2;
  float t1663 = e23y * e1x2;
  float t1664 = e23x * e1z2;
  float t1665 = e23z * e1x2;
  float t1666 = e23y * e1z2;
  float t1667 = e23z * e1y2;
  float t1670 = e1xy * e2x * e3y;
  float t1671 = e1xy * e2y * e3x;
  float t1672 = e1xz * e2x * e3z;
  float t1673 = e1xz * e2z * e3x;
  float t1674 = e1yz * e2y * e3z;
  float t1675 = e1yz * e2z * e3y;
  float t1668 = e1x2 + e1y2 + e1z2 - e13x - e13y - e13z - e12x + e23y - e12y + e23z - e12z -
                t1670 + t1662 - t1671 + t1663 - t1672 + t1664 - t1673 + t1665 - t1674 + t1666 -
                t1675 + e23x + t1667;
  float t1676 = e13x * 2.0;
  float t1677 = e13y * 2.0;
  float t1678 = e13z * 2.0;
  float t1669 = e3x2 + e3z2 + e3y2 + t1560 + e1x2 + t1555 + e1y2 + t1556 + e1z2 + t1557 + t1558 +
                t1559 - t1617 - t1618 - t1619 - t1676 - t1677 - t1678;
  float t1686 = e12x * 2.0;
  float t1687 = e12y * 2.0;
  float t1688 = e12z * 2.0;
  float t1685 = t1550 + t1551 + e2y2 + t1552 + e2z2 + t1553 + e2x2 + t1554 + e1x2 + e1y2 + e1z2 +
                t1549 - t1614 - t1615 - t1616 - t1686 - t1687 - t1688;
  x1 = -g2 * (-e1y * t1592 * t1641 + e2y * t1592 * t1648 + e3y * t1592 * t1652) -
       g3 * (-e1z * t1592 * t1641 + e2z * t1592 * t1648 + e3z * t1592 * t1652) -
       g1 * (-e1x * t1592 * t1641 + e2x * t1592 * t1648 +
             e3x * t1592 *
                 (e2y2 + e2z2 + e2x2 + e13x + e13y + e13z + t1640 + t1635 + t1636 + t1637 + t1638 +
                  t1639 - e12x - e12y - e12z - e23x - e23y - e23z - e1x * e2xy * e3y -
                  e1y * e2xy * e3x - e1x * e2xz * e3z - e1z * e2xz * e3x - e1y * e2yz * e3z -
                  e1z * e2yz * e3y));
  x2 = -g1 * (e1x * t1592 * t1648 - e2x * t1592 * t1669 + e3x * t1592 * t1668) -
       g2 * (e1y * t1592 * t1648 - e2y * t1592 * t1669 + e3y * t1592 * t1668) -
       g3 * (e1z * t1592 * t1648 - e2z * t1592 * t1669 + e3z * t1592 * t1668);
  x3 = -g1 * (e1x * t1592 * t1652 + e2x * t1592 * t1668 - e3x * t1592 * t1685) -
       g2 * (e1y * t1592 * t1652 + e2y * t1592 * t1668 - e3y * t1592 * t1685) -
       g3 * (e1z * t1592 * t1652 + e2z * t1592 * t1668 - e3z * t1592 * t1685);
}

#endif