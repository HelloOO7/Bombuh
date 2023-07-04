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
			socket.SendText(json.dumps({'timer': bomb.timer_int(), 'strikes': bomb.strikes, 'exploded': bomb.has_exploded()}))

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

@MicroWebSrv.route('/api/summary', 'GET')
def apiGetSummary(client, response: MicroWebSrv._response):
	dct = {'exploded': wwwBomb.has_exploded(), 'time_remaining': wwwBomb.timer_int()}
	if (wwwBomb.has_exploded()):
		dct['cause_of_explosion'] = wwwBomb.cause_of_explosion
	response.WriteResponseJSONOk(dct)

@MicroWebSrv.route('/api/exit-app', 'POST')
def apiExitApp(client, response: MicroWebSrv._response):
	wwwBomb.exit()
	response.WriteResponseOk()

@MicroWebSrv.route('/api/discover-modules', 'POST')
def apiDiscoverModules(client, response: MicroWebSrv._response):
	print("Discovering modules...")
	wwwBomb.discover_modules()
	response.WriteResponseOk()

@MicroWebSrv.route('/api/list-modules', 'GET')
def apiListModules(client, response: MicroWebSrv._response):
	ret = []
	for mod in wwwBomb.modules:
		ret.append({'id': mod.id(), 'name': mod.name, 'variables': mod.variables})
	response.WriteResponseJSONOk(ret)

@MicroWebSrv.route('/api/configure', 'POST')
def apiConfigure(client: MicroWebSrv._client, response: MicroWebSrv._response):
	conf = BombConfig()

	form = client.ReadRequestContentAsJSON()
	bad_fields = []
	
	for variable in ['bomb.timer', 'bomb.strikes']:
		if not form.get(variable):
			bad_fields.append(variable)
	
	for comp in wwwBomb.all_components:
		devcfg = ComponentConfig(comp.id())
		for compvar in comp.variables:
			fname = 'module.' + str(comp.id()) + '.' + compvar.name
			if not form.get(fname):
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

@MicroWebSrv.route('/api/configured-check', 'GET')
def apiConfiguredCheck(client: MicroWebSrv._client, response: MicroWebSrv._response):
	response.WriteResponseJSONOk({'configured': wwwBomb.configuration_done()})

@MicroWebSrv.route('/api/start-game', 'POST')
def apiStartGame(client, response: MicroWebSrv._response):
	wwwBomb.arm()
	response.WriteResponseOk()

@MicroWebSrv.route('/api/stop-game', 'POST')
def apiStopGame(client, response: MicroWebSrv._response):
	wwwBomb.reset()
	response.WriteResponseOk()

def _recvTextCallback(webSocket, msg):
	pass

def _recvBinaryCallback(webSocket, data):
	pass

def _closedCallback(webSocket):
	gameMonitor.on_closed(webSocket)

def _acceptWebSocketCallback(webSocket, httpClient):
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
