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
        BOOL: 4
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

        function changeState(newState) {
            if (state) {
                hide(':not(.visible-if-' + newState + ') .visible-if-' + state);
            }
            unhide('.visible-if-' + newState);
            state = newState;
            syncLabelWidths();
        }

        ajaxGet('/api/state').then(function(resp) {
            let loading = document.getElementById('loading');
            let loaded = document.getElementById('loaded');
            if (resp.ok) {
                resp.json().then(function(json) {
                    changeState(json.state);
                    if (json.state == 'INGAME') {
                        startGameMonitor();
                    }
                    loadModules().then(function() {
                        hide(loading);
                        unhide(loaded);
                    });
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

        function loadModules() {
            return ajaxGet('/api/list-modules').then(function(moduleList) {
                if (moduleList.ok) {
                    moduleList.json().then(function(json) {
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
                                    let varDiv = document.createElement('div');

                                    let input = document.createElement('input');
                                    let varid = module.id + '.' + variable.name;
                                    input.id = 'input-' + varid;
                                    input.name = 'module.' + varid;
                                    switch (variable.type) {
                                        case VarType.LONG:
                                        case VarType.INT:
                                            input.type = 'number';
                                            break;
                                        case VarType.STR:
                                            input.type = 'text';
                                            break;
                                        case VarType.BOOL:
                                            input.type = 'checkbox';
                                            break;
                                    }
                                    if (variable.value) {
                                        input.value = variable.value;
                                    }

                                    let label = document.createElement('label');
                                    label.htmlFor = input.id;
                                    label.innerText = variable.name;
                                    
                                    varDiv.appendChild(label);
                                    varDiv.appendChild(input);

                                    fieldset.appendChild(varDiv);
                                });
                                variables.appendChild(fieldset);
                                info.appendChild(variables);
                                ul.appendChild(info);
                            });
                            syncLabelWidths();
                            unhide('.action-configure-bomb');
                        }
                        else {
                            hide('.action-configure-bomb');
                        }
                    });
                }
            });
        }

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
        let timerInterval = null;
        let syncInterval = null;
        let timescale = 1.0;

        function startGameMonitor() {
            //TODO sync timer and strikes over websockets

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

        function setTimer(millis) {
            function zeroPad(num) {
                return num.toString().padStart(2, '0');
            }

            timer = millis;
            if (millis > 60 * 1000) {
                let seconds = Math.floor(millis / 1000)
                timemaj.textContent = zeroPad(Math.floor(seconds / 60));
                timemin.textContent = zeroPad(seconds % 60);
            }
            else {
                timemaj.textContent = zeroPad(Math.floor(millis / 1000));
                timemin.textContent = zeroPad(Math.floor(millis / 10) % 100);
            }
        }

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
            let time = data['bomb.timer'].split(':');
            let timeMillis = parseInt(time[0]) * 60000 + parseInt(time[1]) * 1000;
            data['bomb.timer'] = timeMillis;
            ajaxPost('/api/configure', data).then(function(resp) {
                if (resp.ok) {
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

        addListener('.action-start-game', function(elem) {
            elem.disabled = true;
            ajaxPost('/api/start-game').then(function(resp) {
                elem.disabled = false;
                if (resp.ok) {
                    ajaxGet('/api/game-state').then(function(gs) {
                        gs.json().then(function(json) {
                            setTimer(json.timer);
                            changeState('INGAME');
                            startGameMonitor();
                        });
                    });
                }
            });
        });

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