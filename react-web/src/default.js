let timerId = null;
export const resetSwitches = () => {
    if(!(timerId == null)) {
        clearInterval(timerId);
    }
    
    fetch("./api?set")
    .then(response => response.json())
    .then(alarms => {
        for(let i = 0;i<alarms.length;++i) {
            const id = "a"+i;
            const cb = document.getElementById(id);
            cb.checked = alarms[i];
        }
    })
    .catch(error => console.error("Error fetching JSON data:", error));
    timerId = setInterval(readSwitches,500);
}

export const readSwitches = () => {
    let url = "./api/";
    if(!(timerId == null)) {
        clearInterval(timerId);
    }
    fetch(url)
        .then(response => response.json())
        .then(alarms => {
            for(let i = 0;i<alarms.length;++i) {
                const id = "a"+i;
                const cb = document.getElementById(id);
                if(cb!=undefined) {
                    cb.checked = alarms[i];
                }
            }
        })
        .catch(error => console.error("Error fetching JSON data:", error));
    timerId = setInterval(readSwitches,500);
}

export const writeSwitches = () => {
    if(!(timerId == null)) {
        clearInterval(timerId);
    }
    let url = "./api/";
    url+="?set";
    const cbs = document.getElementsByTagName("input");
    for(let i = 0;i<cbs.length;++i) {
        const id = "a"+i;
        const cb = document.getElementById(id);
        if(cb!=undefined) {
            if(cb.checked) {
                url+=("&a="+i);
            } 
        }
    }
    fetch(url)
    .then(response => response.json())
    .then(alarms => {
        for(let i = 0;i<alarms.length;++i) {
            const id = "a"+i;
            const cb = document.getElementById(id);
            if(cb!=undefined) {
                cb.checked = alarms[i];
            }
        }
    })
    .catch(error => console.error("Error fetching JSON data:", error));
    timerId = setInterval(readSwitches,500);
}
