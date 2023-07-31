from microWebSrv import *
from bomb import *
from microWebSocket import *
import _thread
import json

class WsGameMonitor:
	sockets: list[MicroWebSocket]

	def __init__(self) -> None:
		self.sockets = []

	def open_socket(self, socket: MicroWebSocket):
		self.sockets.append(socket)

	def send_state(self, bomb: Bomb):
		for socket in self.sockets:
			socket.SendText(json.dumps({'timer': bomb.timer_int(), 'strikes': bomb.strikes, 'ended': bomb.has_game_ended()}))

	def close_all(self):
		for socket in self.sockets:
			socket.Close()

	def on_closed(self, socket: MicroWebSocket):
		self.sockets.remove(socket)

wwwBomb: Bomb = None
mwsServer: MicroWebSrv = None
gameMonitor: WsGameMonitor = None

@MicroWebSrv.route('/api/state', 'GET')
def apiGetState(client, response: MicroWebSrv._response):
	response.WriteResponseJSONOk({'state': wwwBomb.state})

@MicroWebSrv.route('/api/game-state', 'GET')
def apiGetGameState(client, response: MicroWebSrv._response):
	response.WriteResponseJSONOk({'timer': wwwBomb.timer_int(), 'strikes': wwwBomb.strikes})

@MicroWebSrv.route('/api/bomb-info', 'GET')
def apiGetBombInfo(client, response: MicroWebSrv._response):
	response.WriteResponseJSONOk({'serial': wwwBomb.serial_number})

@MicroWebSrv.route('/api/summary', 'GET')
def apiGetSummary(client, response: MicroWebSrv._response):
	dct = {'exploded': wwwBomb.has_exploded(), 'time_remaining': wwwBomb.timer_int()}
	if (wwwBomb.has_exploded()):
		dct['cause_of_explosion'] = wwwBomb.cause_of_explosion
	response.WriteResponseJSONOk(dct)

@MicroWebSrv.route('/api/debug-event', 'POST')
def apiDebugEvent(client: MicroWebSrv._client, response: MicroWebSrv._response):
	evtype = client.ReadRequestContentAsJSON()['type']
	if evtype == 'strike':
		wwwBomb.add_strike('Debug event')
	elif evtype == 'defuse':
		wwwBomb.defuse()
	elif evtype == 'explode':
		wwwBomb.explode('Debug event')
	response.WriteResponseOk()

@MicroWebSrv.route('/api/exit-app', 'POST')
def apiExitApp(client, response: MicroWebSrv._response):
	wwwBomb.exit()
	response.WriteResponseOk()

@MicroWebSrv.route('/api/discover-modules', 'POST')
def apiDiscoverModules(client, response: MicroWebSrv._response):
	print("Discovering modules...")
	wwwBomb.discover_modules()
	response.WriteResponseOk()

@MicroWebSrv.route('/api/module-name-dict', 'GET')
def apiModuleNameDict(client, response: MicroWebSrv._response):
	ret = dict()
	for mod in wwwBomb.modules:
		ret[mod.id()] = mod.name

	response.WriteResponseJSONOk(ret)

@MicroWebSrv.route('/api/list-modules', 'GET')
def apiListModules(client, response: MicroWebSrv._response):
	ret = []
	for mod in wwwBomb.modules:
		if mod.name.startswith('$'):
			continue
		ret.append({
			'id': mod.id(), 
			'name': mod.name, 
			'variables': mod.variables, 
			'enum_definitions': mod.enum_definitions
		})
	gc.collect()
	response.WriteResponseJSONOk(ret)

@MicroWebSrv.route('/api/configure', 'POST')
def apiConfigure(client: MicroWebSrv._client, response: MicroWebSrv._response):
	conf = BombConfig()

	form = client.ReadRequestContentAsJSON()
	bad_fields = []

	conf.serial = form.get('bomb.serial') if form.get('bomb.serial') else wwwBomb.generate_serial()
	
	for variable in ['bomb.timer', 'bomb.strikes']:
		if not form.get(variable):
			bad_fields.append(variable)
	
	for comp in wwwBomb.all_components:
		devcfg = ComponentConfig(comp.id())
		for compvar in comp.variables:
			fname = 'module.' + str(comp.id()) + '.' + compvar.name
			if form.get(fname) is None:
				bad_fields.append(fname)
			else:
				devcfg.variables[compvar.name] = form[fname]
		
		conf.component_vars.append(devcfg)

	if (len(bad_fields)):
		response.WriteResponseJSONError(400, {'fields': bad_fields})
	else:
		conf.time_limit_ms = form['bomb.timer']
		conf.strikes = form['bomb.strikes']

		wwwBomb.configure(conf)
		response.WriteResponseOk()

@MicroWebSrv.route('/api/autoconf', 'POST')
def apiAutoConfigure(client: MicroWebSrv._client, response: MicroWebSrv._response):
	request: dict = client.ReadRequestContentAsJSON()
	serial = request.get('serial')
	if not serial:
		serial = wwwBomb.generate_serial()
	config = wwwBomb.generate_config(serial)
	time = request.get('time_limit')
	strikes = request.get('strikes')
	if time:
		config.time_limit_ms = time
	if strikes:
		config.strikes = strikes
	wwwBomb.configure(config)
	response.WriteResponseJSONOk(config)

@MicroWebSrv.route('/api/autoconf-light', 'POST')
def apiAutoConfigureStep(client: MicroWebSrv._client, response: MicroWebSrv._response):
	request: dict = client.ReadRequestContentAsJSON()
	deviceid: int = request.get('device_id')
	on: bool = request.get('is_on')
	print("Send light event dev", deviceid, "on", on)
	wwwBomb.device_event(deviceid, BombEvent.CONFIG_LIGHT, [1 if on else 0])
	response.WriteResponseJSONOk()

@MicroWebSrv.route('/api/configured-check', 'GET')
def apiConfiguredCheck(client: MicroWebSrv._client, response: MicroWebSrv._response):
	response.WriteResponseJSONOk({'configured': wwwBomb.configuration_done()})

@MicroWebSrv.route('/api/start-game', 'POST')
def apiStartGame(client, response: MicroWebSrv._response):
	wwwBomb.arm()
	response.WriteResponseOk()

@MicroWebSrv.route('/api/new-game', 'POST')
def apiNewGame(client, response: MicroWebSrv._response):
	wwwBomb.reset()
	response.WriteResponseOk()

@MicroWebSrv.route('/api/stop-game', 'POST')
def apiStopGame(client, response: MicroWebSrv._response):
	wwwBomb.reset()
	response.WriteResponseOk()

@MicroWebSrv.route('/pair', 'POST')
def webPair(client: MicroWebSrv._client, response: MicroWebSrv._response):
	print("Device", client.GetIPAddr(), "requested pairing.")
	cnt = client.ReadRequestContent()
	data:dict = json.loads(cnt)
	print("Data", cnt)
	if (data.get('device_capabilities') and data.get('network_address')):
		addr = data['network_address']
		for cap in data['device_capabilities']:
			print("Pairing service", cap)
			wwwBomb.add_service(cap, addr)
		response.WriteResponseJSONOk()
	else:
		response.WriteResponseJSONError(400)
	print("Pair done.")

def _recvTextCallback(webSocket, msg):
	pass

def _recvBinaryCallback(webSocket, data):
	pass

def _closedCallback(webSocket):
	global gameMonitor
	if (gameMonitor):
		gameMonitor.on_closed(webSocket)

def _acceptWebSocketCallback(webSocket, httpClient):
	global gameMonitor
	if (gameMonitor):
		gameMonitor.open_socket(webSocket)
		webSocket.RecvTextCallback   = _recvTextCallback
		webSocket.RecvBinaryCallback = _recvBinaryCallback
		webSocket.ClosedCallback     = _closedCallback

def report_game_status():
	global gameMonitor, wwwBomb
	gameMonitor.send_state(wwwBomb)

def start_thread(bomb: Bomb):
	global wwwBomb, mwsServer, gameMonitor
	wwwBomb = bomb
	mws = MicroWebSrv(webPath="/www")
	mws.MaxWebSocketRecvLen = 256
	mws.WebSocketThreaded = True
	mws.AcceptWebSocketCallback = _acceptWebSocketCallback
	mwsServer = mws
	gameMonitor = WsGameMonitor()
	_thread.stack_size(16384)
	mws.Start(threaded=True)

def shutdown():
	global mwsServer, wwwBomb, gameMonitor
	if (mwsServer):
		print("Releasing MWS...")
		mwsServer.Stop()
		gameMonitor.close_all()
		mwsServer = None
		wwwBomb = None
		gameMonitor = None
