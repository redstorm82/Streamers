/*
 *  Copyright (c) 2010 Luca Abeni
 *  Copyright (c) 2010 Csaba Kiraly
 *
 *  This is free software; see gpl-3.0.txt
 */
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>
#include <msg_types.h>
#include <net_helper.h>
#include <topmanager.h>

#include "net_helpers.h"
#include "loop.h"
#include "output.h"
#include "channel.h"

static const char *my_iface = NULL;
#ifdef HTTPIO
int port = 6666;
#else
static int port = 6666;
#endif
static int srv_port;
static const char *srv_ip = "";
static int period = 40;
static int chunks_per_second = 25;
static int multiply = 1;
static int buff_size = 50;
static int outbuff_size = 25;
static const char *fname = "input.mpg";
static bool loop_input = false;
unsigned char msgTypes[] = {MSG_TYPE_TOPOLOGY,MSG_TYPE_CHUNK,MSG_TYPE_SIGNALLING};

static void print_usage()
{
  fprintf (stderr,
    "Usage:offerstreamer [-bocmtpiPIflCh]\n"
    "\n"
    "Peer options\n"
    "\t[-p port]: port of the remote peer to connect at during bootstrap.\n"
    "\t           Usually it is the source peer port.\n"
    "\t[-i IP]: IP address of the remote peer to connect at during bootstrap.\n"
    "\t         Usually it is the source peer IP\n"
    "\t[-C name]: set the channel name to use on the repository.\n"
    "\t           All peers should use the same channel name.\n"
    "\n"
    "\t[-b size]: set the peer Chunk Buffer size.\n"
    "\t           This is also the chunk trading window size.\n"
    "\t[-o size]: set the Output Buffer size.\n"
    "\t[-c chunks]: set the number of chunks a peer can send per seconds.\n"
    "\t             it controls the upload capacity of peer as well.\n"
    "\t[-t time]: chunk emission period. STILL NEEDED??\n"
    "\t[-P port]: local UDP port to be used by the peer.\n"
    "\t[-I IP]: local IP address to be used by the peer.\n"
    "\t         Useful if the host has several interfaces/addresses.\n"
    "\n"
    "Special Source Peer options\n"
    "\t[-m chunks]: set the number of copies the source injects in the overlay.\n"
    "\t[-f filename]: name of the video stream file to transmit.\n"
    "\t[-l]: loop the video stream.\n"
    "\n"
    "NOTE: the peer will dump the received video on STDOUT in raw format\n"
    "      it can be played by your favourite player simply using a pipe\n"
    "      e.g., | vlc -\n"
    "\n"
    "Examples:\n"
    "\n"
    "Start a source peer on port 6600:\n"
    "\n"
    "./offestreamer -m 3 -C MyTest -l -f foreman.avi -P 6600\n"
    "\n"
    "Start a peer connecting to the previous source, and using videolan as player:\n"
    "\n"
    "./offerstreamer -i 130.192.9.140 -p 6600 |vlc -\n"
    "\n"
    
    );
  }



static void cmdline_parse(int argc, char *argv[])
{
  int o;

  while ((o = getopt(argc, argv, "b:o:c:t:p:i:P:I:f:m:lC:")) != -1) {
    switch(o) {
      case 'b':
        buff_size = atoi(optarg);
        break;
      case 'o':
        outbuff_size = atoi(optarg);
        break;
        case 'c':
        chunks_per_second = atoi(optarg);
        break;
      case 'm':
        multiply = atoi(optarg);
        break;
      case 't':
        period = atoi(optarg);
        break;
      case 'p':
        srv_port = atoi(optarg);
        break;
      case 'i':
        srv_ip = strdup(optarg);
        break;
      case 'P':
        port =  atoi(optarg);
        break;
      case 'I':
        my_iface = strdup(optarg);
        break;
      case 'f':
        fname = strdup(optarg);
        break;
      case 'l':
        loop_input = true;
        break;
      case 'C':
        channel_set_name(optarg);
        break;
      default:
        fprintf(stderr, "Error: unknown option %c\n", o);
        print_usage();

        exit(-1);
    }
  }

  if (!channel_get_name()) {
    channel_set_name("generic");
  }
}

static struct nodeID *init(void)
{
  int i;
  struct nodeID *myID;
  char *my_addr;

  if (my_iface) {
    my_addr = iface_addr(my_iface);
  } else {
    my_addr = default_ip_addr();
  }

  if (my_addr == NULL) {
    fprintf(stderr, "Cannot find network interface %s\n", my_iface);

    return NULL;
  }
  for (i=0;i<3;i++)
	  bind_msg_type(msgTypes[i]);
  myID = net_helper_init(my_addr, port);
  if (myID == NULL) {
    fprintf(stderr, "Error creating my socket (%s:%d)!\n", my_addr, port);
    free(my_addr);

    return NULL;
  }
  free(my_addr);
  topInit(myID, NULL, 0);

  output_init(outbuff_size);

  return myID;
}


int main(int argc, char *argv[])
{
  struct nodeID *my_sock;

  cmdline_parse(argc, argv);

  my_sock = init();
  if (my_sock == NULL) {
    return -1;
  }
  if (srv_port != 0) {
    struct nodeID *srv;

    srv = create_node(srv_ip, srv_port);
    if (srv == NULL) {
      fprintf(stderr, "Cannot resolve remote address %s:%d\n", srv_ip, srv_port);

      return -1;
    }
    topAddNeighbour(srv, NULL, 0);

    loop(my_sock, 1000000 / chunks_per_second, buff_size);
  }

  source_loop(fname, my_sock, period * 1000, multiply, loop_input);

  return 0;
}
