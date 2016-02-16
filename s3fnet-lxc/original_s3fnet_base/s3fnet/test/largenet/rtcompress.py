import sys
from collections import deque

# Outputs CIDR addresses in decreasing mask length if True
REV_OUTPUT = True
#REV_OUTPUT = False

# Outputs a second DML file with CIDR addresses in the opposite order specified if True
DOUBLE_OUTPUT = True
#DOUBLE_OUTPUT = False

DEBUG = True
DEBUG_V = False
WHITESPACE = [' ','\t','\n','\r']
IP_SIZE = 32 # change for ipv6

# currently unbalanced, just need a simple implementation
class IPTrie(object):
	def __init__(self,destip = None, interface = None, nexthop = None):
		self.next_hop_table = []
		self.default_interface = None
		self.right = None
		self.left = None
		self.__nhindex = 0
		if destip != None and interface != None:
			self.insert(destip,interface,nexthop)

	def insert(self,destip,interface,nexthop):
		if destip == 'default':
			self.default_interface = interface
			return
		#TODO: Maintain a count of unique instances of next_hops; use to choose CIDR mappings
		#TODO: Note, this also requires maintaining a list of indices (vs single nh-index)
		try:
			index = self.next_hop_table.index([interface,nexthop])
		except ValueError:
			self.next_hop_table.append([interface,nexthop])
			index = self.__nhindex
			self.__nhindex+=1
		binip = ip2bin(destip)
		node = self
		for i in range(len(binip)):
			if binip[i] == '1':
				if node.right == None:
					node.right = IPTrieNode(binip[:i+1],index)
				node = node.right
			else: # binip[i] == '0'
				if node.left == None:
					node.left = IPTrieNode(binip[:i+1],index)
				node = node.left

	def assemble_cidr(self):
		queue = deque()
		if self.left != None:
			queue.append(self.left)
		if self.right != None:
			queue.append(self.right)
		routes = []
		while len(queue) > 0:
			temp_routes = self.__bfs_assemble_routes(queue)
			routes.extend(temp_routes)
		if self.default_interface != None:
			routes.append(["default",self.default_interface,None])
		return routes

	def __bfs_assemble_routes(self,queue):
		if len(queue) == 0:
			return []
		node = queue.popleft()
		routes = []
		if node.index != node.active_index: # ignore routes already covered by higher CIDR schemes
			destip = ip2string(int(node.binip,2),len(node.binip))
			next_hop = self.next_hop_table[node.index]
			routes.append([destip,next_hop[0],next_hop[1]])
		if node.left != None:
			queue.append(node.left)
			node.left.active_index = node.index
		if node.right != None:
			queue.append(node.right)
			node.right.active_index = node.index
		return routes

class IPTrieNode(object):
	#TODO: Can optimize out 'binip field
	def __init__(self,binip,nh_index):
		self.binip = binip
		self.index = nh_index
		self.active_index = None
		self.left = None
		self.right = None

def ip2bin(ipstring):
	parts = ipstring.split('.')
	res = ""
	for i in range(len(parts)):
		temp = bin(int(parts[i]))[2:]
		padding = 8-len(temp)
		res += padding*'0' + temp
	return res

def ip2int(ipstring):
	parts = ipstring.split('.')
	retval = 0
	lp = len(parts)
	for i in range(lp):
		retval += int(parts[i]) << (lp-i-1)*8
	return retval

def ip2string(ip,prefix = IP_SIZE):
	if ip == 0:
		for i in range(IP_SIZE/8):
			if i==0:
				retip = "0"
			else:
				retip+=".0"
	else:
		ip = ip << (IP_SIZE-prefix)
		retip = ""
		count = 0
		while ip > 0:
			temp = ip & 255
			if retip == "":
				retip = str(temp)
			else:
				retip = str(temp)+"."+retip
			ip = ip >> 8
			count+=8
		while count < IP_SIZE:
			retip = "0."+retip
			count+=8
	if prefix < IP_SIZE:
		retip += "/"+str(prefix)
	return retip

def load_env(filename = "test-env.dml"):
	ipmap = {}
	try:
		infile = open(filename)
	except IOError:
		print("Error: Could not open "+filename)
		return ipmap
	# TODO: Currently does not support multi-line interface definitions
	if DEBUG:
		print("Loading env data...")
	for line in infile:
		istart = line.find("interface")
		if istart == -1:
			continue
		istart+=10 # len(interface)+len("[") == 10
		nhi = get_dml_val(line,"nhi",istart)
		if nhi == None:
			continue
		ip = get_dml_val(line,"ip",istart)
		if ip == None:
			continue
		ipmap[nhi] = ip
	return ipmap

def load_rt(ipmap, filename = "test-rt.dml",output_filename = "test-rt2.dml"):
	routable_dest = []
	try:
		infile = open(filename)
	except IOError:
		print("Error: Could not open "+filename)
		return
	# TODO: Currently expects 'forwarding_table', each entry, and ']' on their own lines
	curtable = None
	nhi = ""
	first = True
	counter = 0
	for line in infile:
		if curtable == None: # if not in a forwarding_table
			# TODO: REQUIRES routable destinations to be listed before any forwarding table
			rd = get_dml_val(line,"routable destination:")
			if rd != None:
				routable_dest.append(rd)
				continue
			fstart = line.find("forwarding_table")
			if fstart == -1:
				continue
			nhi = get_dml_val(line,"node_nhi",fstart+17)
			if nhi == None:
				nhi = ""
				continue
			elif DEBUG and counter % 100 == 0:
				print("Converting nhi "+str(counter)+"/"+str(len(routable_dest))+" ("+nhi+")")
			counter += 1
			curtable = []
		else:
			rstart = line.find("nhi_route")
			if rstart == -1:
				if line.find("]") != -1:
					newroutes = rtcompress(curtable,ipmap)
					output_routes(nhi, newroutes, routable_dest, output_filename, first)
					if first:
						first = False
					del curtable
					curtable = None
				continue
			rstart+=10
			dest = get_dml_val(line,"dest",rstart)
			if dest[0] == ':': # this represents and absolute address
				dest = dest[1:] # TODO: resolve address vs discard colon
			if dest == None:
				continue
			interface = get_dml_val(line,"interface",rstart)
			if interface == None:
				continue
			next_hop = get_dml_val(line,"next_hop",rstart)
			# NOTE: next_hop may be None for default interface
			curtable.append([dest,interface,next_hop])
	return

def rtcompress(routes,ipmap):
	if DEBUG_V:
		print("Compressing routes to CIDR form...")
	patricia_trie = IPTrie()
	for entry in routes:
		if entry[0] == "default":
			patricia_trie.insert(entry[0],entry[1],entry[2])
		else:
			patricia_trie.insert(ipmap[entry[0]],entry[1],entry[2])
	temp_routes = patricia_trie.assemble_cidr()
	return temp_routes

# TODO: This is not optimal (but it works) - redundant code for DOUBLE_OUTPUT
def output_routes(nhi, routes, routable_dest, filename, first = True):
	try:
		if first:
			outfile = open(filename,"w")
		else:
			outfile = open(filename,"a")
	except IOError:
		print("Error: Could not open "+filename+" for write.")
		return
	if DEBUG_V:
		print("Writing routes for "+nhi+" to "+filename+"...")
	if first:
		for rd in routable_dest:
			outfile.write("#routable destination: "+rd+"\n")
	outfile.write("forwarding_table [ node_nhi "+nhi+"\n")
	# Quick fix - routes must be written in reverse order to prevent overwriting duplicate routes in s3fnet
	if REV_OUTPUT:
		start = len(routes)-1
		end = -1
		step = -1
	else:
		start = 0
		end = len(routes)
		step = 1
	for i in range(start,end,step):
		route = routes[i]
		outfile.write("  nhi_route [ dest")
		if route[0] == "default":
			outfile.write(" default interface "+route[1]+" ]\n")
		else:
			outfile.write("ip "+route[0]+" interface "+route[1]+" next_hop "+route[2]+" ]\n")
	outfile.write("]\n")
	outfile.close()
	if DOUBLE_OUTPUT:
		try:
			if first:
				outfile2 = open(filename+".rev","w")
			else:
				outfile2 = open(filename+".rev","a")
		except IOError:
			print("Error: Could not open "+filename+".rev"+" for write.")
			return
		if first:
			for rd in routable_dest:
				outfile2.write("#routable destination: "+rd+"\n")
		outfile2.write("forwarding_table [ node_nhi "+nhi+"\n")
		if not REV_OUTPUT:
			start = len(routes)-1
			end = -1
			step = -1
		else:
			start = 0
			end = len(routes)
			step = 1
		for i in range(start,end,step):
			route = routes[i]
			outfile2.write("  nhi_route [ dest")
			if route[0] == "default":
				outfile2.write(" default interface "+route[1]+" ]\n")
			else:
				outfile2.write("ip "+route[0]+" interface "+route[1]+" next_hop "+route[2]+" ]\n")
		outfile2.write("]\n")
		outfile2.close()

# TODO: Does not properly handle shared-text keys (nhi vs node_nhi, etc)
def get_dml_val(line,key,startat=0):
	start = line.find(key,startat)
	if start == -1:
		return None
	start+=len(key)
	while line[start] in WHITESPACE:
		start+=1
	end = start+1
	while end < len(line) and line[end] not in WHITESPACE:
		end+=1
	return line[start:end]

# testing/debugging
def dump_ipmap(ipmap,filename="ipmap.txt"):
	try:
		outfile = open(filename,"w")
	except IOError:
		print("Error: Could not open "+filename+" for write.")
		return
	if DEBUG:
		print("Dumping ipmap...")
	for key in ipmap:
		outfile.write(key + " : " + ipmap[key]+"\n")
	outfile.close()
	return

def dump_routes(routes,filename="routes.txt"):
	try:
		outfile = open(filename,"w")
	except IOError:
		print("Error: Could not open "+filename+" for write.")
		return
	if DEBUG:
		print("Dumping routes to "+filename+"...")
	for key in routes:
		outfile.write("NHI: "+key+"\n")
		for entry in routes[key]:
			outfile.write("\t"+entry[0]+", "+entry[1])
			if entry[2] != None:
				outfile.write(", "+entry[2])
			outfile.write("\n")
	outfile.close()
	return

if __name__ == "__main__":
	#TODO: add flag parsing
	if len(sys.argv) > 1:
		ipmap = load_env(sys.argv[1])
	else:
		ipmap = load_env()
	#if DEBUG:
	#	dump_ipmap(ipmap)
	# All functionality has now been moved into load_rt to lower mem overhead
	if len(sys.argv) > 2:
		load_rt(ipmap,sys.argv[2])
	else:
		load_rt(ipmap)
