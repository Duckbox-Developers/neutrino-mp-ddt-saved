/*
 * $port: configure_network.cpp,v 1.7 2009/11/20 22:44:19 tuxbox-cvs Exp $
 *
 * (C) 2003 by thegoodguy <thegoodguy@berlios.de>
 * (C) 2011 Stefan Seyfried
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <config.h>
#include <cstdio>               /* perror... */
#include <sys/wait.h>
#include <sys/types.h>          /* u_char */
#include <string.h>
#include <unistd.h>
#include "configure_network.h"
#include <lib/libnet/libnet.h>             /* netGetNameserver, netSetNameserver   */
#include <lib/libnet/network_interfaces.h> /* getInetAttributes, setInetAttributes */
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <system/helpers.h>

CNetworkConfig::CNetworkConfig()
{
	netGetNameserver(nameserver);
	ifname = "eth0";
	orig_automatic_start = false;
	orig_inet_static = false;
	automatic_start = false;
	inet_static = false;
}

CNetworkConfig* CNetworkConfig::getInstance()
{
	static CNetworkConfig* network_config = NULL;

	if(!network_config)
	{
		network_config = new CNetworkConfig();
		printf("[network config] Instance created\n");
	}
	return network_config;
}

CNetworkConfig::~CNetworkConfig()
{
}

void CNetworkConfig::readConfig(std::string iname)
{
	ifname = iname;
	inet_static = getInetAttributes(ifname, automatic_start, address, netmask, broadcast, gateway);

	init_vars();
	copy_to_orig();
}

void CNetworkConfig::init_vars(void)
{
	std::string mask;
	std::string _broadcast;
	std::string router;
	std::string ip;
	unsigned char addr[6];

	netGetHostname(hostname);

	netGetDefaultRoute(router);
	gateway = router;

	/* FIXME its enough to read IP for dhcp only ?
	 * static config should not be different from settings in etc/network/interfaces */
	if(!inet_static) {
		netGetIP(ifname, ip, mask, _broadcast);
		netmask = mask;
		broadcast = _broadcast;
		address = ip;
	}

	netGetMacAddr(ifname, addr);

	std::stringstream mac_tmp;
	for(int i=0;i<6;++i)
		mac_tmp<<std::hex<<std::setfill('0')<<std::setw(2)<<(int)addr[i]<<':';

	mac_addr = mac_tmp.str().substr(0,17);
}

void CNetworkConfig::copy_to_orig(void)
{
	orig_automatic_start = automatic_start;
	orig_address         = address;
	orig_netmask         = netmask;
	orig_broadcast       = broadcast;
	orig_gateway         = gateway;
	orig_inet_static     = inet_static;
	orig_hostname	     = hostname;
	orig_ifname	     = ifname;
}

bool CNetworkConfig::modified_from_orig(void)
{
#ifdef DEBUG
		if(orig_automatic_start != automatic_start)
			printf("CNetworkConfig::modified_from_orig: automatic_start changed\n");
		if(orig_address         != address        )
			printf("CNetworkConfig::modified_from_orig: address changed\n");
		if(orig_netmask         != netmask        )
			printf("CNetworkConfig::modified_from_orig: netmask changed\n");
		if(orig_broadcast       != broadcast      )
			printf("CNetworkConfig::modified_from_orig: broadcast changed\n");
		if(orig_gateway         != gateway        )
			printf("CNetworkConfig::modified_from_orig: gateway changed\n");
		if(orig_hostname        != hostname       )
			printf("CNetworkConfig::modified_from_orig: hostname changed\n");
		if(orig_inet_static     != inet_static    )
			printf("CNetworkConfig::modified_from_orig: inet_static changed\n");
		if(orig_ifname	      != ifname)
			printf("CNetworkConfig::modified_from_orig: ifname changed\n");
#endif
	/* check for following changes with dhcp enabled trigger apply question on menu quit, 
	 * even if apply already done */
	if (inet_static) {
		if ((orig_address         != address        ) ||
		    (orig_netmask         != netmask        ) ||
		    (orig_broadcast       != broadcast      ) ||
		    (orig_gateway         != gateway        ))
			return 1;
	}
	return (
		(orig_automatic_start != automatic_start) ||
		(orig_hostname        != hostname       ) ||
		(orig_inet_static     != inet_static    ) ||
		(orig_ifname	      != ifname)
		);

#if 0
	return (
		(orig_automatic_start != automatic_start) ||
		(orig_address         != address        ) ||
		(orig_netmask         != netmask        ) ||
		(orig_broadcast       != broadcast      ) ||
		(orig_gateway         != gateway        ) ||
		(orig_hostname        != hostname       ) ||
		(orig_inet_static     != inet_static    ) ||
		(orig_ifname	      != ifname)
		);
#endif
}

void CNetworkConfig::commitConfig(void)
{
	if (modified_from_orig())
	{
		if(orig_hostname != hostname)
			netSetHostname(hostname);

		if (inet_static)
		{
			addLoopbackDevice("lo", true);
			setStaticAttributes(ifname, automatic_start, address, netmask, broadcast, gateway);
		}
		else
		{
			addLoopbackDevice("lo", true);
			setDhcpAttributes(ifname, automatic_start);
		}

		copy_to_orig();

	}
	if (nameserver != orig_nameserver)
	{
		orig_nameserver = nameserver;
		netSetNameserver(nameserver);
	}
}

void CNetworkConfig::startNetwork(void)
{
	std::string cmd = "/sbin/ifup " + ifname;
#ifdef DEBUG
	printf("CNetworkConfig::startNetwork: %s\n", cmd.c_str());
#endif
	my_system(3, "/bin/sh", "-c", cmd.c_str());

	if (!inet_static) {
		init_vars();
	}
	//mysystem((char *) "ifup",  (char *) "-v",  (char *) "eth0");
}

void CNetworkConfig::stopNetwork(void)
{
	std::string cmd = "/sbin/ifdown " + ifname;
#ifdef DEBUG
	printf("CNetworkConfig::stopNetwork: %s\n", cmd.c_str());
#endif
	my_system(3, "/bin/sh", "-c", cmd.c_str());

}
