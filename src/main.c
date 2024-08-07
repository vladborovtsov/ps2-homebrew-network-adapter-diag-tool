/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
*/

#include <stdio.h>
#include <kernel.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <debug.h>
#include <netman.h>
#include <ps2ip.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <sbv_patches.h>

extern unsigned char DEV9_irx[];
extern unsigned int size_DEV9_irx;

extern unsigned char SMAP_irx[];
extern unsigned int size_SMAP_irx;

extern unsigned char NETMAN_irx[];
extern unsigned int size_NETMAN_irx;

static int ethApplyNetIFConfig(int mode)
{
	int result;
	//By default, auto-negotiation is used.
	static int CurrentMode = NETMAN_NETIF_ETH_LINK_MODE_AUTO;

	if(CurrentMode != mode)
	{	//Change the setting, only if different.
		if((result = NetManSetLinkMode(mode)) == 0)
			CurrentMode = mode;
	}else
		result = 0;

	return result;
}

static void EthStatusCheckCb(s32 alarm_id, u16 time, void *common)
{
	iWakeupThread(*(int*)common);
}

static int WaitValidNetState(int (*checkingFunction)(void))
{
	int ThreadID, retry_cycles;

	// Wait for a valid network status;
	ThreadID = GetThreadId();
	for(retry_cycles = 0; checkingFunction() == 0; retry_cycles++)
	{	//Sleep for 1000ms.
		SetAlarm(1000 * 16, &EthStatusCheckCb, &ThreadID);
		SleepThread();

		if(retry_cycles >= 10)	//10s = 10*1000ms
			return -1;
	}

	return 0;
}

static int ethGetDHCPStatus(void)
{
        t_ip_info ip_info;
        int result;

        if ((result = ps2ip_getconfig("sm0", &ip_info)) >= 0)
        {       //Check for a successful state if DHCP is enabled.
                if (ip_info.dhcp_enabled)
                        result = (ip_info.dhcp_status == DHCP_STATE_BOUND || (ip_info.dhcp_status == DHCP_STATE_OFF));
                else
                        result = -1;
        }

        return result;
}

static int ethWaitValidDHCPState(void)
{
        return WaitValidNetState(&ethGetDHCPStatus);
}

static void my_sleep(int seconds) {
    usleep(1000 * 1000 * seconds);
}

static int ethGetNetIFLinkStatus(void)
{
	return(NetManIoctl(NETMAN_NETIF_IOCTL_GET_LINK_STATUS, NULL, 0, NULL, 0) == NETMAN_NETIF_ETH_LINK_STATE_UP);
}

static int ethWaitValidNetIFLinkState(void)
{
	return WaitValidNetState(&ethGetNetIFLinkStatus);
}

static int ethApplyIPConfig(int use_dhcp, const struct ip4_addr *ip, const struct ip4_addr *netmask, const struct ip4_addr *gateway, const struct ip4_addr *dns)
{
	t_ip_info ip_info;
	int result;

	//SMAP is registered as the "sm0" device to the TCP/IP stack.
	if ((result = ps2ip_getconfig("sm0", &ip_info)) >= 0)
	{
		const ip_addr_t *dns_curr;

		//Obtain the current DNS server settings.
		dns_curr = dns_getserver(0);

		//Check if it's the same. Otherwise, apply the new configuration.
		if ((use_dhcp != ip_info.dhcp_enabled)
		    ||	(!use_dhcp &&
			 (!ip_addr_cmp(ip, (struct ip4_addr *)&ip_info.ipaddr) ||
			 !ip_addr_cmp(netmask, (struct ip4_addr *)&ip_info.netmask) ||
			 !ip_addr_cmp(gateway, (struct ip4_addr *)&ip_info.gw) ||
			 !ip_addr_cmp(dns, dns_curr))))
		{
			if (use_dhcp)
			{
				ip_info.dhcp_enabled = 1;
			}
			else
			{	//Copy over new settings if DHCP is not used.
				ip_addr_set((struct ip4_addr *)&ip_info.ipaddr, ip);
				ip_addr_set((struct ip4_addr *)&ip_info.netmask, netmask);
				ip_addr_set((struct ip4_addr *)&ip_info.gw, gateway);

				ip_info.dhcp_enabled = 0;
			}

			//Update settings.
			result = ps2ip_setconfig(&ip_info);
			if (!use_dhcp)
				dns_setserver(0, dns);
		}
		else
			result = 0;
	}

	return result;
}

static void ethPrintIPConfig(void)
{
	t_ip_info ip_info;
	u8 ip_address[4], netmask[4], gateway[4], dns[4];

	//SMAP is registered as the "sm0" device to the TCP/IP stack.
	if (ps2ip_getconfig("sm0", &ip_info) >= 0)
	{
		const ip_addr_t *dns_curr;

		//Obtain the current DNS server settings.
		dns_curr = dns_getserver(0);

		ip_address[0] = ip4_addr1((struct ip4_addr *)&ip_info.ipaddr);
		ip_address[1] = ip4_addr2((struct ip4_addr *)&ip_info.ipaddr);
		ip_address[2] = ip4_addr3((struct ip4_addr *)&ip_info.ipaddr);
		ip_address[3] = ip4_addr4((struct ip4_addr *)&ip_info.ipaddr);

		netmask[0] = ip4_addr1((struct ip4_addr *)&ip_info.netmask);
		netmask[1] = ip4_addr2((struct ip4_addr *)&ip_info.netmask);
		netmask[2] = ip4_addr3((struct ip4_addr *)&ip_info.netmask);
		netmask[3] = ip4_addr4((struct ip4_addr *)&ip_info.netmask);

		gateway[0] = ip4_addr1((struct ip4_addr *)&ip_info.gw);
		gateway[1] = ip4_addr2((struct ip4_addr *)&ip_info.gw);
		gateway[2] = ip4_addr3((struct ip4_addr *)&ip_info.gw);
		gateway[3] = ip4_addr4((struct ip4_addr *)&ip_info.gw);

		dns[0] = ip4_addr1(dns_curr);
		dns[1] = ip4_addr2(dns_curr);
		dns[2] = ip4_addr3(dns_curr);
		dns[3] = ip4_addr4(dns_curr);

		scr_printf(	"IP:\t%d.%d.%d.%d\n"
				"NM:\t%d.%d.%d.%d\n"
				"GW:\t%d.%d.%d.%d\n"
				"DNS:\t%d.%d.%d.%d\n",
					ip_address[0], ip_address[1], ip_address[2], ip_address[3],
					netmask[0], netmask[1], netmask[2], netmask[3],
					gateway[0], gateway[1], gateway[2], gateway[3],
					dns[0], dns[1], dns[2], dns[3]);
	}
	else
	{
		scr_printf("Unable to read IP address.\n");
	}
}

static void ethPrintLinkStatus(void)
{
	int mode, baseMode;

	//SMAP is registered as the "sm0" device to the TCP/IP stack.
	int status = NetManIoctl(NETMAN_NETIF_IOCTL_GET_LINK_STATUS, NULL, 0, NULL, 0);

	scr_printf("Link:\t");
	if (status == NETMAN_NETIF_ETH_LINK_STATE_UP)
		scr_printf("Up\n");
	else
		scr_printf("Down\n");

	scr_printf("Status code: %d\n", status);
	scr_printf("Mode:\t");
	mode = NetManIoctl(NETMAN_NETIF_IOCTL_ETH_GET_LINK_MODE, NULL, 0, NULL, 0);

	//NETMAN_NETIF_ETH_LINK_MODE_PAUSE is a flag, so file it off first.
	baseMode = mode & (~NETMAN_NETIF_ETH_LINK_DISABLE_PAUSE);
	switch(baseMode)
	{
		case NETMAN_NETIF_ETH_LINK_MODE_10M_HDX:
			scr_printf("10M HDX");
			break;
		case NETMAN_NETIF_ETH_LINK_MODE_10M_FDX:
			scr_printf("10M FDX");
			break;
		case NETMAN_NETIF_ETH_LINK_MODE_100M_HDX:
			scr_printf("100M HDX");
			break;
		case NETMAN_NETIF_ETH_LINK_MODE_100M_FDX:
			scr_printf("100M FDX");
			break;
		default:
			scr_printf("Unknown");
	}
	if(!(mode & NETMAN_NETIF_ETH_LINK_DISABLE_PAUSE))
		scr_printf(" with ");
	else
		scr_printf(" without ");
	scr_printf("Flow Control\n");
}

int main(int argc, char *argv[])
{
	//Reboot IOP
	SifInitRpc(0);
	while(!SifIopReset("", 0)){};
	while(!SifIopSync()){};

	//Initialize SIF services
	SifInitRpc(0);
	SifLoadFileInit();
	SifInitIopHeap();
	sbv_patch_enable_lmb();

	//Load the required modules
	SifExecModuleBuffer(DEV9_irx, size_DEV9_irx, 0, NULL, NULL);
	SifExecModuleBuffer(NETMAN_irx, size_NETMAN_irx, 0, NULL, NULL);
	SifExecModuleBuffer(SMAP_irx, size_SMAP_irx, 0, NULL, NULL);

	//Initialize NETMAN
	NetManInit();

	init_scr();

    scr_printf("\n\n\n----- PS2 Network Adapter Diagnostic Tool\n");
	scr_printf("-----------------------------------------\n\n");

    u8 macAddress[6];

    // Use NetManIoctl to get the MAC address
	scr_printf("Will try to read adapter's mac address...\n");
    if (NetManIoctl(NETMAN_NETIF_IOCTL_ETH_GET_MAC, NULL, 0, macAddress, sizeof(macAddress)) < 0) {
        scr_printf("Failed to get MAC address. Is adapter attached?\n");
        my_sleep(10);
		goto end;
    }

    // Print the MAC address
    scr_printf("MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n",
           macAddress[0], macAddress[1], macAddress[2],
           macAddress[3], macAddress[4], macAddress[5]);

    

	// Ethernet link mode
	scr_printf("\nWill try to set ethernet link mode: AUTO\n");	
	int EthernetLinkMode = NETMAN_NETIF_ETH_LINK_MODE_AUTO;

	if(ethApplyNetIFConfig(EthernetLinkMode) != 0) {
		scr_printf("Error: failed to set link mode.\n");
		goto end;
	}
	
    struct ip4_addr IP, NM, GW, DNS;
	// For dhcp we need zeroes. 
	ip4_addr_set_zero(&IP);
	ip4_addr_set_zero(&NM);
    ip4_addr_set_zero(&GW);
    ip4_addr_set_zero(&DNS);

	ps2ipInit(&IP, &NM, &GW);
	ethApplyIPConfig(1, &IP, &NM, &GW, &DNS);

	//Wait for the link to become ready.
	scr_printf("Waiting for connection...\n");
	int attmpt = 0; 

	while (attmpt < 10) {
	 	if(ethWaitValidNetIFLinkState() != 0) {
			scr_printf("Error: failed to get valid link status. Waiting... #%d\n", attmpt);
			attmpt++;
		}
		break;
	}

	if (attmpt >=10) {
		scr_printf("Attempts exceeded. Exiting.\n");
		goto end;
	}

	scr_printf("Waiting for DHCP lease...");
    if (ethWaitValidDHCPState() != 0)
    {
        scr_printf("DHCP failed\n.");
        goto end;
    }
	

	scr_printf("Initialized:\n");
	ethPrintLinkStatus();
	ethPrintIPConfig();

	//At this point, network support has been initialized and the PS2 can be pinged.
	scr_printf("Init complete. At this point PS2 can be pinged on the IP shown above.");
	SleepThread();

  end:
	ps2ipDeinit();
	NetManDeinit();
	SifExitRpc();
	return 0;
}