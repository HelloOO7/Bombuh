from microWebSrv import *
from bomb import *
import _thread
import json

wwwBomb: Bomb = None
mwsServer: MicroWebSrv = None

@MicroWebSrv.route('/api/state', 'GET')
def apiGetState(client, response: MicroWebSrv._response):
	response.WriteResponseJSONOk({'state': wwwBomb.state})

@MicroWebSrv.route('/api/game-state', 'GET')
def apiGetGameState(client, response: MicroWebSrv._response):
	response.WriteResponseJSONOk({'timer': wwwBomb.timer_int(), 'strikes': wwwBomb.strikes})

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

def start_thread(bomb: Bomb):
	global wwwBomb, mwsServer
	wwwBomb = bomb
	mws = MicroWebSrv(webPath="/www")
	mwsServer = mws
	_thread.stack_size(32768)
	mws.Start(threaded=True)

def shutdown():
	global mwsServer, wwwBomb
	if (mwsServer):
		print("Releasing MWS...")
		mwsServer.Stop()
		mwsServer = None
		wwwBomb = None
