let timerId = null;
export const resetSwitches = () => {
    fetch("./api?set")
        .then(response => response.json())
        .then(alarms => {
            for (let i = 0; i < alarms.length; ++i) {
                const id = "a" + i;
                const cb = document.getElementById(id);
                cb.checked = alarms[i];
            }
        })
        .catch(error => console.error("Error fetching JSON data:", error));
}
const socket = new WebSocket("ws://" + document.location.hostname + ":" + document.location.port + "/socket");
export const setSwitchesJson = () => {
    let url = "./api/";
    url += "?set";
    const cbs = document.getElementsByTagName("input");
    for (let i = 0; i < cbs.length; ++i) {
        const id = "a" + i;
        const cb = document.getElementById(id);
        if (cb != undefined) {
            if (cb.checked) {
                url += ("&a=" + i);
            }
        }
    }
    fetch(url)
        .then(response => response.json())
        .then(alarms => {
            for (let i = 0; i < alarms.length; ++i) {
                const id = "a" + i;
                const cb = document.getElementById(id);
                if (cb != undefined) {
                    cb.checked = alarms[i];
                }
            }
        })
        .catch(error => console.error("Error fetching JSON data:", error));
}
export const setSwitches = () => {
    const values = []
    for (let i = 0; i < 32; ++i) {
        const id = "a" + i;
        const cb = document.getElementById(id);
        if (cb != undefined) {
            values.push(cb.checked);
        } else {
            break;
        }
    }
    const buf = new ArrayBuffer(5);
    const view = new DataView(buf,0);
    view.setUint8(0,values.length);
    let packed = 0;
    for(let i = values.length-1;i>=0;--i) {
        packed <<= 1;
        if(values[i]) {
            packed|=1;
        }
    }
    view.setUint32(1,packed,false);
    socket.send(buf);
}

export const connectSwitches = () => {

    socket.binaryType = "arraybuffer";
    // Connection opened
    socket.addEventListener("open", event => {
        console.log("connected");
    });

    // Listen for messages
    socket.addEventListener("message", event => {
        let res = [];
        if (event.data instanceof ArrayBuffer) {
            const view = new DataView(event.data);
            if (view.byteLength != 5) {
                console.log("Byte length of " + view.byteLength + " unexpected");
            } else {
                const count = view.getUint8(0);
                let data = view.getUint32(1, false);
                for (let i = 0; i < count; ++i) {
                    if (((data >>> i) % 2) != 0) {
                        res.push(true);
                    } else {
                        res.push(false);
                    }
                }
                for (let i = 0; i < count; ++i) {
                    const id = "a" + i;
                    const cb = document.getElementById(id);
                    if (cb != undefined) {
                        cb.checked = res[i];
                    }
                }
            }
        }
    });
}