"""Bilingual layout, preset workflow and high-resolution analyzer regression checks."""
import importlib.util
import json
from pathlib import Path
from playwright.sync_api import sync_playwright

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("webui", root / "tests/validate-webui.py")
webui = importlib.util.module_from_spec(spec)
spec.loader.exec_module(webui)
out = root / "vst3/build/ui-refresh-qa"
out.mkdir(parents=True, exist_ok=True)
failures, report = [], []
url = (root / "vst3/WebUI/index.html").as_uri()
with sync_playwright() as pw:
    browser = pw.chromium.launch(executable_path=str(webui.find_browser(None)), headless=True)
    for language in ("zh", "en"):
        for width, height in webui.VIEWPORTS:
            page = browser.new_page(viewport={"width":width,"height":height})
            page.add_init_script(f"localStorage.setItem('flipshift.language', '{language}')")
            page.on("pageerror", lambda error: failures.append(str(error)))
            page.goto(url)
            page.wait_for_timeout(250)
            assert page.locator("#analyzerLayout").input_value() == "overlay"
            assert page.locator("#quality").input_value() == "2"
            metrics = webui.collect_metrics(page)
            webui.validate_metrics((width,height), metrics, failures)
            page.screenshot(path=str(out / f"refresh-{language}-{width}x{height}.png"))
            report.append({"language":language,"size":[width,height],"metrics":metrics})
            if (width,height) == (320,280):
                page.locator("#compactControlTab").click()
                page.screenshot(path=str(out / f"refresh-{language}-320x280-controls.png"))
            page.close()
    page = browser.new_page(viewport={"width":1000,"height":650}, device_scale_factor=2)
    page.add_init_script(webui.MOCK_BRIDGE_SCRIPT)
    page.goto(url)
    assert page.locator("html").get_attribute("lang") == "zh-CN"
    page.locator("#languageSelect").select_option("en")
    assert page.locator("#shiftHzLabel").inner_text() == "OFFSET Hz"
    page.reload()
    assert page.locator("html").get_attribute("lang") == "en"
    page.locator("#languageSelect").select_option("zh")
    page.evaluate("window.__emitFromCpp('presetState', {id:'init',name:'Init',dirty:true,entries:[{id:'init',name:'Init'},{id:'test',name:'测试预设'}]})")
    page.locator("#presetSelect").select_option("test")
    assert page.locator("#presetDialog").is_visible()
    page.locator('#presetDialog button[value="cancel"]').click()
    assert page.locator("#presetSelect").input_value() == "init"
    assert not page.evaluate("window.__bridgeEvents.some(e=>e.name==='presetCommand' && e.payload.action==='load')")
    page.locator("#presetSelect").select_option("test")
    page.locator("#presetDiscard").click()
    page.wait_for_timeout(30)
    command = page.evaluate("window.__bridgeEvents.filter(e=>e.name==='presetCommand').at(-1).payload")
    assert command["action"] == "load" and command["id"] == "test"
    page.evaluate("p=>window.__emitFromCpp('presetResult',{requestId:p.requestId,ok:false,error:'preset.values'})",command)
    assert "无效" in page.locator("#presetNotice").inner_text()
    assert page.locator("#presetSelect").input_value() == "init"
    page.locator("#presetMenu summary").click()
    page.locator('[data-preset-action="saveAs"]').click()
    page.locator("#presetName").fill("我的频谱 01")
    assert page.evaluate("(() => { const e=new Event('selectstart',{bubbles:true,cancelable:true}); return document.getElementById('presetName').dispatchEvent(e); })()")
    page.locator("#presetConfirm").click()
    page.wait_for_timeout(30)
    command = page.evaluate("window.__bridgeEvents.filter(e=>e.name==='presetCommand').at(-1).payload")
    assert command["action"] == "saveAs" and command["name"] == "我的频谱 01"
    page.evaluate("p=>{window.__emitFromCpp('presetState',{id:'test',name:p.name,dirty:false,entries:[{id:'init',name:'Init'},{id:'test',name:p.name}]});window.__emitFromCpp('presetResult',{requestId:p.requestId,ok:true});}",command)
    assert page.locator("#presetSelect").input_value() == "test"
    assert not page.locator("#presetDirty").is_visible()
    page.evaluate("""() => {
      const a=Array.from({length:1024},(_,i)=>-90+70*Math.exp(-Math.pow((i-600)/8,2)));
      window.__emitFromCpp('analyzerFrame',{input:a,output:a});
    }""")
    page.evaluate("""() => {
      window.__emitFromCpp('waterfallFrame', {fftSize:8192, output:Array.from({length:1024},(_,i)=>Math.abs(i-420)<2 || Math.abs(i-440)<2 ? -12 : -96)});
    }""")
    page.wait_for_timeout(200)
    assert page.locator("#waterfallCanvas").get_attribute("data-analysis-fft-size") == "8192"
    assert page.locator("#waterfallCanvas").evaluate("""e => {
      const ctx=e.getContext('2d');
      const pixel = n => ctx.getImageData(e.width-2,Math.round((1-n/1023)*(e.height-1)),1,1).data;
      const peak=pixel(420), valley=pixel(430);
      return Math.max(...peak.slice(0,3)) > Math.max(...valley.slice(0,3)) + 30;
    }""")
    page.screenshot(path=str(out / "refresh-zh-dpr2-native-mock.png"))
    assert page.locator("#waterfallCanvas").evaluate("e=>e.width") > 1000
    before = page.locator("#waterfallCanvas").get_attribute("data-advance-count")
    page.locator("#languageSelect").select_option("en")
    after = page.locator("#waterfallCanvas").get_attribute("data-advance-count")
    assert int(after) >= int(before)
    browser.close()
(out / "refresh-validation.json").write_text(json.dumps({"failures":failures,"viewports":report},ensure_ascii=False,indent=2),encoding="utf-8")
if failures:
    raise AssertionError("\n".join(failures))
print("PASS: bilingual five-viewport layout, 2x DPI, language persistence, preset save/cancel/error workflow, editable input and history retention")
