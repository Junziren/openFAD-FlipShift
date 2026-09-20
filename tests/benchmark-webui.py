"""Compare the current UI with the git HEAD baseline at the same High/Overlay settings."""
import importlib.util, json, subprocess, time
from pathlib import Path
from playwright.sync_api import sync_playwright
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("webui",root/"tests/validate-webui.py")
webui=importlib.util.module_from_spec(spec);spec.loader.exec_module(webui)
baseline=root/"vst3/build/ui-baseline";baseline.mkdir(exist_ok=True)
for name in ("index.html","app.js","styles.css"):
    (baseline/name).write_bytes(subprocess.check_output(["git","show",f"HEAD:vst3/WebUI/{name}"]))
report=[]
with sync_playwright() as pw:
    browser=pw.chromium.launch(executable_path=str(webui.find_browser(None)),headless=True)
    for width,height,dpr in ((1000,650,1),(1600,800,2)):
        for label,folder,points in (("baseline",baseline,192),("current",root/"vst3/WebUI",1024)):
            page=browser.new_page(viewport={"width":width,"height":height},device_scale_factor=dpr)
            page.add_init_script(webui.MOCK_BRIDGE_SCRIPT)
            page.goto((folder/"index.html").as_uri())
            page.locator("#analyzerLayout").select_option("overlay")
            page.evaluate("window.__emitFromCpp('parameterState',{values:{quality:2},sampleRate:48000})")
            page.evaluate("""points=>{
                window.__benchTimes=[];let prev=performance.now(),frame=0;
                const measure=now=>{window.__benchTimes.push(now-prev);prev=now;window.__benchRaf=requestAnimationFrame(measure)};
                window.__benchRaf=requestAnimationFrame(measure);
                window.__benchTimer=setInterval(()=>{
                    const a=Array.from({length:points},(_,i)=>-90+70*Math.exp(-Math.pow((i/points-0.4-0.15*Math.sin(frame/25))/0.015,2)));
                    window.__emitFromCpp('analyzerFrame',{input:a,output:a});frame++;
                },1000/15);
            }""",points)
            page.wait_for_timeout(700)
            cdp=page.context.new_cdp_session(page);cdp.send("Performance.enable")
            before={x['name']:x['value'] for x in cdp.send('Performance.getMetrics')['metrics']}
            page.evaluate('window.__benchTimes=[]')
            page.wait_for_timeout(3000)
            after={x['name']:x['value'] for x in cdp.send('Performance.getMetrics')['metrics']}
            intervals=sorted(page.evaluate('window.__benchTimes'))
            report.append({'version':label,'viewport':[width,height],'dpr':dpr,'points':points,
                'rendererTaskMsPerSecond':(after['TaskDuration']-before['TaskDuration'])*1000/3,
                'rafP95Ms':intervals[int(len(intervals)*.95)],'rafMaxMs':max(intervals),
                'jsHeapMB':after['JSHeapUsedSize']/1024/1024})
            page.close()
    browser.close()
(root/'vst3/build/ui-refresh-qa/performance.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))
