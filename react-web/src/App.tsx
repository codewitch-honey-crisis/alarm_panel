import { useEffect,Suspense } from "react";
import './App.css'
import { resetSwitches,readSwitches,writeSwitches } from './default.js';
const fetchData = () => {
  let data;
  const promise = fetch("./api/")
    .then((response) => response.json())
    .then((json) => (data = json));
  return {
    read() {
      if (!data) {
        throw promise; 
      }
      return data;
    },
  };
};
const alarmData = fetchData();
function Alarm({index}) {
    return (
        <>
        <label className="switchLabel">{index+1}</label>
        <label className="switch">
            <input id={"a"+index} type="checkbox" name="a" value={index} onClick={writeSwitches} />
            <span className="slider round"></span>
        </label>
        <span>&nbsp;&nbsp;</span>
        </>
    )
}
const Alarms = () => {
  const status = alarmData.read();
  const alarms = [];
    for(let i: number = 0;i<status.length;++i) {
        alarms.push(<Alarm index={i} />);
    }
    return (<form method="get" action="#">{alarms}</form>)

};
export default function App() {
  useEffect(() => {
    readSwitches();
}, []);
    return (
    <Suspense fallback={<p>waiting for data...</p>}>
        <>
            <h1>Alarm Control Panel</h1>
            <Alarms />
            <br />
            <button className="button" onClick={resetSwitches} type="button">Reset All</button>
        </>
    </Suspense>
  );
}
