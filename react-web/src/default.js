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
const requestSwitches = () => {
    let ab = new ArrayBuffer(1);
    socket.send(ab);
}
export const setSwitches = () => {
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