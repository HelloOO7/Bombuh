(function() {
    const ajax = async function (url, method, obj, extra = {}) {
        let mergeExtra = function (init) {
            for (let prop in extra) {
                if (extra.hasOwnProperty(prop)) {
                    init[prop] = extra[prop];
                }
            }
            return init;
        };
    
        let response;
        if (method === 'GET' || method === 'HEAD') {
            response = await fetch(url + (url.includes('?') ? '&' : '?') + new URLSearchParams(obj),
                mergeExtra({
                    method: method,
                })
            );
        } else {
            response = await fetch(url,
                mergeExtra({
                    headers: {
                        'X-HTTP-Method-Override': method
                    },
                    method: 'POST',
                    body: (obj instanceof FormData) ? obj : JSON.stringify(obj)
                })
            );
        }
    
        if (typeof phpdebugbar !== 'undefined') {
            phpdebugbar.ajaxHandler.handle(response);
        }
        return response;
    }
    
    const ajaxGet = function (url, obj, extra = {}) {
        return ajax(url, 'GET', obj, extra);
    }
    
    const ajaxPost = function (url, obj, extra = {}) {
        return ajax(url, 'POST', obj, extra);
    }

    const VarType = {
        NULL: 0,
        STR: 1,
        INT: 2,
        LONG: 3,
        BOOL: 4,
        STR_ENUM: 5
    };

    function unhide(selector) {
        if (typeof(selector) == 'string') {
            document.querySelectorAll(selector).forEach(function(e) {
                e.classList.remove('hidden');
            });
        }
        else {
            selector.classList.remove('hidden');
        }
    }

    function hide(selector) {
        if (typeof(selector) == 'string') {
            document.querySelectorAll(selector).forEach(function(e) {
                e.classList.add('hidden');
            });
        }
        else {
            selector.classList.add('hidden');
        }
    }

    function addListener(selector, func) {
        document.querySelectorAll(selector).forEach(function (elem) {
            elem.addEventListener('click', function (e) {
                e.preventDefault();
                func(elem);
                return true;
            });
        });
    }

    document.addEventListener('DOMContentLoaded', function(e) {
        let state = null;

        function endLoading() {
            let loading = document.getElementById('loading');
            let loaded = document.getElementById('loaded');
            hide(loading);
            unhide(loaded);
        }

        function changeState(newState) {
            function done() {
                unhide('.visible-if-' + newState);
                syncLabelWidths();
                endLoading();
            }

            if (state) {
                hide(':not(.visible-if-' + newState + ') .visible-if-' + state);
            }
            state = newState;
            if (state != 'INGAME') {
                closeGameMonitor();
            }
            else {
                startGameMonitor(done);
            }
            if (state == 'SUMMARY') {
                fillSummaryScreen().then(done);
            }
            if (state == 'IDLE') {
                loadModules().then(done);
            }
        }

        function convTimerInput(text) {
            time = text.split(':');
            return parseInt(time[0]) * 60000 + parseInt(time[1]) * 1000;
        }

        function createVarEditElement(module, variable) {
            let varDiv = document.createElement('div');

            let input = document.createElement('input');
            switch (variable.type) {
                case VarType.LONG:
                case VarType.INT:
                    input.type = 'number';
                    if (variable.value) input.value = variable.value;
                    break;
                case VarType.STR:
                    input.type = 'text';
                    input.value = variable.value;
                    break;
                case VarType.STR_ENUM:
                    input = document.createElement('select');
                    module.enum_definitions[variable.extra].forEach(function(enumconst) {
                        let option = document.createElement('option');
                        option.textContent = enumconst;
                        if (enumconst == variable.value) {
                            option.selected = true;
                        }
                        input.appendChild(option);
                    });
                    break;
                case VarType.BOOL:
                    input.type = 'checkbox';
                    input.checked = variable.value;
                    break;
            }

            let varid = module.id + '.' + variable.name;
            input.id = 'input-' + varid;
            input.name = 'module.' + varid;

            let label = document.createElement('label');
            label.htmlFor = input.id;
            label.innerText = variable.name;
            
            varDiv.appendChild(label);
            varDiv.appendChild(input);
            
            return varDiv;
        }

        ajaxGet('/api/state').then(function(resp) {
            if (resp.ok) {
                resp.json().then(function(json) {
                    changeState(json.state);
                });
            }
            else {
                loading.innerHTML = 'Chyba načítání. (' + resp.status + ')';
            }
        });

        let cont = document.getElementById('container');

        function ensureClearBlock(parent, id) {
            let blk = document.getElementById(id);
            if (!blk) {
                blk = document.createElement('div');
                blk.id = id;
                parent.appendChild(blk);
            }
            else {
                blk.innerHTML = '';
            }
            return blk;
        }

        function syncLabelWidths() {
            let fieldsets = document.querySelectorAll('fieldset');
            fieldsets.forEach(function(fieldset) {
                let labels = fieldset.querySelectorAll('label');
                let mw = 0;
                labels.forEach(function(lab) {
                    lab.style.width = null;
                });
                labels.forEach(function(lab) {
                    if (lab.offsetWidth > mw) {
                        mw = lab.offsetWidth;
                    }
                });
                if (mw > 0) {
                    labels.forEach(function(lab) {
                        lab.style.width = (mw + 5) + 'px';
                        lab.style.display = 'inline-block';
                    });
                }
            });
        }

        addListener('.action-exit-app', function(elem) {
            ajaxPost('/api/exit-app').then(function(resp) {
                if (resp.ok) {
                    document.body.innerHTML = 'Konec.';
                }
            });
        });

        function clearExtraContainer() {
            document.getElementById('configuration-extra-container').innerHTML = '';
        }

        function loadModules() {
            return ajaxGet('/api/list-modules').then(function(moduleList) {
                if (moduleList.ok) {
                    moduleList.json().then(function(json) {
                        clearExtraContainer();
                        let moduleBlock = ensureClearBlock(document.getElementById('configuration-extra-container'), 'modulelist');
                        if (json.length) {
                            let title = document.createElement('h3');
                            title.textContent = 'Moduly';
                            moduleBlock.appendChild(title);
                            let ul = document.createElement('ul');
                            moduleBlock.appendChild(ul);
                            json.forEach(function(module) {
                                let info = document.createElement('li');
                                info.dataset.module = module.id;
                                info.innerText = module.name;
                                let variables = document.createElement('div');
                                variables.classList.add('margin');
                                let fieldset = document.createElement('fieldset');
                                fieldset.style.borderStyle = 'none';
                                module.variables.forEach(function(variable) {
                                    fieldset.appendChild(createVarEditElement(module, variable));
                                });
                                variables.appendChild(fieldset);
                                info.appendChild(variables);
                                ul.appendChild(info);
                            });
                            syncLabelWidths();
                            unhide('.action-configure-bomb');
                        }
                        else {
                            //hide('.action-configure-bomb');
                        }
                    });
                }
            });
        }

        addListener('.action-debug-event', function(elem) {
            ajaxPost('/api/debug-event', {'type': elem.dataset.event});
        });

        addListener('.action-discover-modules', function(elem) {
            hide('.action-start-game');
            let oldText = elem.textContent;
            elem.disabled = true;
            elem.textContent = 'Hledání zařízení...'
            ajaxPost('/api/discover-modules').then(function(resp) {
                elem.disabled = false;
                elem.textContent = oldText;
                if (!resp.ok) {
                    alert('Chyba: ' + JSON.stringify(resp));
                }
                else {
                    loadModules();
                }
            });
        });

        let timemaj = document.getElementById('timer-major');
        let timemin = document.getElementById('timer-minor');

        let timer = 5 * 60 * 1000;
        let rendered_timer = 0;
        let timerInterval = null;
        let syncInterval = null;
        let timescale = 1.0;
        let websocket = null;
        let strikes = 0;

        function closeGameMonitor() {
            console.log("Close game monitor.");
            if (timerInterval !== null) {
                clearInterval(timerInterval);
                timerInterval = null;
            }
            if (websocket) {
                websocket.close();
                websocket = null;
            }
        }

        function fillSummaryScreen() {
            let content = document.getElementById('summary-content');
            hide(content);
            return ajaxGet('/api/summary').then(function(resp) {
                if (resp.ok) {
                    resp.json().then(function(json) {
                        if (json.exploded) {
                            hide('#summary-defused');
                            unhide('#summary-exploded');

                            document.getElementById('cause-of-explosion').textContent = json.cause_of_explosion;
                        }
                        else {
                            unhide('#summary-defused');
                            hide('#summary-exploded');
                        }
                        let millis = json.time_remaining;
                        document.getElementById('time-remaining').textContent = formatTimeMM(millis) + ":" + formatTimeSS(millis);
                        console.log(content);
                        unhide(content);
                    });
                }
            });
        }

        function startGameMonitor(callback = null) {
            closeGameMonitor();
            
            let semaphore = 0;
            function notifySemaphore() {
                semaphore--;
                if (semaphore == 0 && callback) {
                    callback();
                }
            }
            
            semaphore++;
            ajaxGet('/api/bomb-info').then(function(resp) {
                if (resp.ok) {
                    resp.json().then(function(json) {
                        document.getElementById('bomb-serial').textContent = json.serial;
                        notifySemaphore();
                    });
                }
            });
            console.log("Open game monitor.");
            if (websocket) {
                console.log("WS already open????");
            }
            websocket = new WebSocket('ws://' + window.location.host + '/game-monitor');
            
            websocket.onclose = function(ev) {
                websocket = null;
            };

            let firstMessage = true;

            semaphore++;
            websocket.onmessage = function(ev) {
                let data = JSON.parse(ev.data);
                if (data.ended) {
                    changeState('SUMMARY');
                }
                else {
                    if (firstMessage) {
                        notifySemaphore();
                        firstMessage = false;
                    }
                    setStrikes(data.strikes);
                    setTimer(data.timer);
                }
            };

            lastTimerIntervalTs = 0;
            timerInterval = setInterval(() => {
                let time = Date.now();
                if (!lastTimerIntervalTs) {
                    lastTimerIntervalTs = time;
                }
                else {
                    let newTime = timer - timescale * (time - lastTimerIntervalTs);
                    if (newTime < 0) {
                        newTime = 0;
                        clearInterval(timerInterval);
                    }
                    setTimer(newTime);
                    lastTimerIntervalTs = time;
                }
            }, 50);
        }

        function resetTimer() {
            rendered_timer = 0;
        }

        let strikesEl = document.getElementById('strikes-xs');

        function setStrikes(nr) {
            strikes = nr;
            timescale_table = [1.0, 1.25, 1.5, 3.0, 6.0];
            timescale = timescale_table[Math.min(nr, timescale_table.length - 1)];
            strikesEl.textContent = 'X'.repeat(nr);
        }

        function resetTimerAndStrikes() {
            resetTimer();
            setStrikes(0);
        }

        function zeroPad(num) {
            return num.toString().padStart(2, '0');
        }

        function formatTimeMM(millis) {
            let seconds = Math.floor(millis / 1000);
            return zeroPad(Math.floor(seconds / 60));
        }

        function formatTimeSS(millis) {
            let seconds = Math.floor(millis / 1000);
            return zeroPad(seconds % 60);
        }

        function setTimer(millis) {
            if (millis < 0) {
                millis = 0;
            }

            timer = millis;
            if (!rendered_timer || timer < rendered_timer) {
                rendered_timer = timer
                if (millis > 60 * 1000) {
                    timemaj.textContent = formatTimeMM(millis);
                    timemin.textContent = formatTimeSS(millis);
                }
                else {
                    timemaj.textContent = zeroPad(Math.floor(millis / 1000));
                    timemin.textContent = zeroPad(Math.floor(millis / 10) % 100);
                }
            }
        }

        addListener('.action-new-game', function(elem) {
            ajaxPost('/api/new-game').then(function(resp) {
                if (resp.ok) {
                    changeState('IDLE');
                }
            });
        });

        addListener('.action-configure-bomb', function(elem) {
            elem.disabled = true;
            let form = document.getElementById('configuration-form');
            let data = {};
            form.querySelectorAll('input').forEach(function(input) {
                input.classList.remove('incorrect');
                switch (input.type) {
                    case 'hidden':
                    case 'text':
                    default:
                        data[input.name] = input.value;
                        break;
                    case 'number':
                        data[input.name] = parseInt(input.value);
                        break;
                    case 'checkbox':
                        data[input.name] = input.checked;
                        break;
                }
            });
            form.querySelectorAll('select').forEach(function(input) {
                input.classList.remove('incorrect');
                data[input.name] = input.value;
            });
            let time = data['bomb.timer']
            data['bomb.timer'] = convTimerInput(time);
            ajaxPost('/api/configure', data).then(function(resp) {
                if (resp.ok) {
                    resetTimerAndStrikes();
                    setTimer(timeMillis);
                    function configCheck() {
                        ajaxGet('/api/configured-check').then(function(resp) {
                            if (resp.ok) {
                                resp.json().then(function(json) {
                                    if (json.configured) {
                                        elem.disabled = false;
                                        unhide('.action-start-game');
                                    }
                                    else {
                                        setTimeout(configCheck, 500);
                                    }
                                });
                            }
                            else {
                                elem.disabled = false;
                            }
                        });
                    };
                    configCheck();
                    unhide('.action-start-game');
                }
                else if (resp.status == 400) {
                    elem.disabled = false;
                    resp.json().then(function(json) {
                        json.fields.forEach(function(culprit) {
                            let elem = form.querySelector('input[name="' + culprit + '"]');
                            if (elem) {
                                elem.classList.add('incorrect');
                            }
                        });
                    });
                }
            });
        });

        addListener('.action-autoconf', function(elem) {
            elem.disabled = true;
            hide('.action-configure-bomb');
            let timer = convTimerInput(document.getElementById('conf-bomb-time').value);
            let serial = document.getElementById('conf-bomb-serial').value;
            let strikes = parseInt(document.getElementById('conf-bomb-strikes').value);

            ajaxPost('/api/autoconf', {
                time_limit: timer,
                serial: serial,
                strikes: strikes
            }).then(function(resp) {
                if (resp.ok) {
                    resp.json().then(function(jsonconfig) {
                        let moduledict = {};

                        function autoconfWiz(config) {
                            let compConfigs = config.component_vars;
                            let nonemptyConfigs = [];
                            compConfigs.forEach(function(elem) {
                                if (Object.keys(elem.variables).length > 0) {
                                    nonemptyConfigs.push(elem);
                                }
                            });
                            compConfigs = nonemptyConfigs;

                            clearExtraContainer();
                            let wizardContainer = ensureClearBlock(document.getElementById('configuration-extra-container'), 'wizard');

                            let index = 0;
                            compConfigs.forEach(function(compcfg) {
                                let compDiv = document.createElement('div');
                                compDiv.classList.add('wizard-step');
                                if (index > 0) {
                                    hide(compDiv);
                                }
                                compDiv.dataset.device_id = compcfg.id.toString();
                                compDiv.dataset.index = (index++).toString();
                                let fieldset = document.createElement('fieldset');
                                let legend = document.createElement('legend');
                                legend.textContent = moduledict[compcfg.id];
                                fieldset.appendChild(legend);

                                for (const [varname, varvalue] of Object.entries(compcfg.variables)) {
                                    let value = document.createElement('span');
                                    value.style.display = 'inline-block';
                                    let varid = compcfg.id + '.' + varname;
                                    value.id = 'value-' + varid;
                                    value.textContent = varvalue;
                                    value.style.paddingLeft = '15px';

                                    let label = document.createElement('label');
                                    label.htmlFor = value.id;
                                    label.innerText = varname + ': ';

                                    let varDiv = document.createElement('div');
                                    varDiv.appendChild(label);
                                    varDiv.appendChild(value);

                                    fieldset.appendChild(varDiv);
                                }
                                
                                compDiv.appendChild(fieldset);
                                wizardContainer.appendChild(compDiv);
                            });

                            let controls = document.createElement('div');
                            let curStep = -1;
                            let curDevice = null;

                            function changeWizardStep(index) {
                                if (curStep >= 0) {
                                    ajaxPost('/api/autoconf-light', {'device_id': curDevice, 'is_on': false});
                                }
                                document.querySelectorAll('.wizard-step').forEach(function(step) {
                                    if (index == step.dataset.index) {
                                        unhide(step);
                                        curDevice = parseInt(step.dataset.device_id);
                                        ajaxPost('/api/autoconf-light', {'device_id': curDevice, 'is_on': true});
                                    }
                                    else {
                                        hide(step);
                                    }
                                });
                                curStep = index;
                                if (curStep == compConfigs.length) {
                                    btnNext.disabled = true;
                                    btnPrev.disabled = true;
                                    clearExtraContainer();
                                    ensureClearBlock(document.getElementById('configuration-extra-container'), 'readyscreen').innerHTML =
                                    `
                                    <div id="ready-timer">
                                        <h1>${formatTimeMM(config.time_limit_ms)}:${formatTimeSS(config.time_limit_ms)}</h1>
                                    </div>
                                    <button type="button" class="action-start-game" id="ready-start-game">Start hry</button>
                                    `;
                                    addListener('#ready-start-game', doStartGame);
                                }
                            }
                            
                            let btnPrev = document.createElement('button');
                            btnPrev.type = 'button';
                            btnPrev.textContent = 'Předchozí';
                            btnPrev.onclick = function() {
                                changeWizardStep(getWizardStep() - 1);
                                updateButtons();
                            };

                            let btnNext = document.createElement('button');
                            btnNext.type = 'button';
                            btnNext.onclick = function() {
                                changeWizardStep(getWizardStep() + 1);
                                updateButtons();
                            };
                            
                            function getWizardStep() {
                                let e = document.querySelector('.wizard-step:not(.hidden)');
                                return e ? parseInt(e.dataset.index) : compConfigs.length;
                            }

                            function updateButtons() {
                                let step = getWizardStep();
                                if (step == compConfigs.length - 1) {
                                    btnNext.textContent = 'Dokončit';
                                }
                                else {
                                    btnNext.textContent = 'Další';
                                }
                                btnPrev.disabled = step == 0;
                            }

                            controls.append(btnPrev);
                            controls.append(btnNext);

                            updateButtons();
                            changeWizardStep(0);
                            
                            wizardContainer.appendChild(controls);
                            syncLabelWidths();
                        }

                        function configCheck() {
                            ajaxGet('/api/configured-check').then(function(resp) {
                                if (resp.ok) {
                                    resp.json().then(function(json) {
                                        if (json.configured) {
                                            ajaxGet('/api/module-name-dict').then(function(resp) {
                                                resp.json().then(function(json) {
                                                    moduledict = json;
                                                    elem.disabled = false;
                                                    autoconfWiz(jsonconfig);
                                                });
                                            });
                                        }
                                        else {
                                            setTimeout(configCheck, 500);
                                        }
                                    });
                                }
                            });
                        };
                        configCheck();
                    });
                }
            });
        });

        function doStartGame(elem) {
            elem.disabled = true;
            ajaxPost('/api/start-game').then(function(resp) {
                elem.disabled = false;
                if (resp.ok) {
                    ajaxGet('/api/game-state').then(function(gs) {
                        gs.json().then(function(json) {
                            resetTimerAndStrikes();
                            setTimer(json.timer);
                            changeState('INGAME');
                        });
                    });
                }
            });
        }
        
        addListener('.action-start-game', doStartGame);

        addListener('.action-stop-game', function(elem) {
            elem.disabled = true;
            ajaxPost('/api/stop-game').then(function(resp) {
                if (timerInterval) {
                    clearInterval(timerInterval);
                }
                elem.disabled = false;
                if (resp.ok) {
                    changeState('IDLE');
                }
            });
        });
    });
})();