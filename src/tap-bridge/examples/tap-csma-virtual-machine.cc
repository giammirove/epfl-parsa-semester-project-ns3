/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

//
// This is an illustration of how one could use virtualization techniques to
// allow running applications on virtual machines talking over simulated
// networks.
//
// The actual steps required to configure the virtual machines can be rather
// involved, so we don't go into that here.  Please have a look at one of
// our HOWTOs on the nsnam wiki for more details about how to get the
// system configured.  For an example, have a look at "HOWTO Use Linux
// Containers to set up virtual networks" which uses this code as an
// example.
//
// The configuration you are after is explained in great detail in the
// HOWTO, but looks like the following:
//
//  +----------+                           +----------+
//  | virtual  |                           | virtual  |
//  |  Linux   |                           |  Linux   |
//  |   Host   |                           |   Host   |
//  |          |                           |          |
//  |   eth0   |                           |   eth0   |
//  +----------+                           +----------+
//       |                                      |
//  +----------+                           +----------+
//  |  Linux   |                           |  Linux   |
//  |  Bridge  |                           |  Bridge  |
//  +----------+                           +----------+
//       |                                      |
//  +------------+                       +-------------+
//  | "tap-left" |                       | "tap-right" |
//  +------------+                       +-------------+
//       |           n0            n1           |
//       |       +--------+    +--------+       |
//       +-------|  tap   |    |  tap   |-------+
//               | bridge |    | bridge |
//               +--------+    +--------+
//               |  CSMA  |    |  CSMA  |
//               +--------+    +--------+
//                   |             |
//                   |             |
//                   |             |
//                   ===============
//                      CSMA LAN
//
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/network-module.h"
#include "ns3/tap-bridge-module.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <thread>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TapCsmaVirtualMachineExample");

CsmaHelper csma;

void
createSimulation(int delay, int n_nodes)
{
    //
    // We are interacting with the outside, real, world.  This means we have to
    // interact in real-time and therefore means we have to use the real-time
    // simulator and take the time to calculate checksums.
    //
    GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(false));

    //
    // Create two ghost nodes.  The first will represent the virtual machine host
    // on the left side of the network; and the second will represent the VM on
    // the right side.
    //
    NodeContainer nodes;
    nodes.Create(n_nodes);

    //
    // Use a CsmaHelper to get a CSMA channel created, and the needed net
    // devices installed on both of the nodes.  The data rate and delay for the
    // channel can be set through the command-line parser.  For example,
    //
    // ./ns3 run "tap-csma-virtual-machine --ns3::CsmaChannel::DataRate=10000000"
    //
    // DataRate x("10Gbps");
    // double nBits = x*ns3::Seconds(19.2);
    // uint32_t nBytes = 20;
    // double txtime = x.CalclulateTxTime(nBytes);
    // csma.SetChannelAttribute("DataRate", DataRateValue(x));
    // csma.SetChannelAttribute("DataRate", DataRateValue(10000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(delay)));
    NetDeviceContainer devices = csma.Install(nodes);

    //
    // Use the TapBridgeHelper to connect to the pre-configured tap devices for
    // the left side.  We go with "UseBridge" mode since the CSMA devices support
    // promiscuous mode and can therefore make it appear that the bridge is
    // extended into ns-3.  The install method essentially bridges the specified
    // tap to the specified CSMA device.
    //
    TapBridgeHelper tapBridge;
    tapBridge.SetAttribute("Mode", StringValue("UseLocal"));

    for (int i = 0; i < n_nodes; i++)
    {
        char str[100];
        sprintf(str, "tap%d-ns", i);
        tapBridge.SetAttribute("DeviceName", StringValue(str));
        tapBridge.Install(nodes.Get(i), devices.Get(i));
    }

    // Simulator::Stop(Seconds(600.));
    Simulator::Run();
    Simulator::Destroy();
}

void
ns3task(int delay, int n_nodes)
{
    createSimulation(delay, n_nodes);
}

void
input()
{
    char in[1024];
    for (;;)
    {
        scanf("%s", in);
        printf("%s\n", in);
    }
}

pthread_t native_handler;

void
start_thread(int delay, int n_nodes)
{
    std::thread thrd(ns3task, delay, n_nodes);
    native_handler = thrd.native_handle();
    thrd.detach();
}

void
stop_thread()
{
    pthread_cancel(native_handler);
    Simulator::Stop();
    Simulator::Destroy();
}

void
restart_thread(int delay, int n_nodes)
{
    stop_thread();
    start_thread(delay, n_nodes);
}

int
main(int argc, char* argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    int n_nodes = 3;
    int delay = 0;
    start_thread(delay, n_nodes);

    char in[1024];
    for (;;)
    {
        printf("> ");
        scanf("%s", in);
        if (strcmp(in, "stop") == 0)
        {
            printf("stopping\n");
            stop_thread();
        }
        if (strcmp(in, "chgd") == 0)
        {
            printf("delay > ");
            scanf("%d", &delay);
            restart_thread(delay, n_nodes);
        }
        if (strcmp(in, "chgn") == 0)
        {
            printf("n nodes > ");
            scanf("%d", &n_nodes);
            restart_thread(delay, n_nodes);
        }
        in[0] = 0;
    }

    return 0;
}
