import sys , os
import importlib
import urllib
import json
import FrameGraph

FrameGraph, Matcher = FrameGraph.FrameGraph, FrameGraph.Matcher


class DataProcess:

    def __init__(self):
        self._json_dic = {}
        pass

    def ImportJsonFile(self, file):
        jsonData = json.load(file)
        return jsonData
        pass

    def ImportJsonString(self, content):
        jsonData = json.loads(content)
        return jsonData
        pass

class DataViewer:
    def __init__(self):

        pass

    #
    def Handle_JsonData(self, call_satck, root):
        if type(root) != list:
            print("root is not a list type")
            return

        length = len(root)
        if length <= 0 :
            return

        index = 0
        while index < length:
            call_info = root[index]
            strdesc = call_info["mod"]+call_info["func"]+u" line:"+str(call_info["line"])+u" -def:"+str(call_info["def"])
            call_satck.Enter( strdesc)
            call_satck.Update(call_info["t"]*1000)
            if "sub" in call_info:
                self.Handle_JsonData(call_satck, call_info["sub"])
                pass

            call_satck.Leave()
            index = index + 1
        pass


def GenerateHtml(jsonfile, index):
    if not os.path.exists(jsonfile):
        print("input file path is not exists")
        return


    ifile = open(jsonfile, 'r', encoding='utf-8')
    content = ifile.read()
    content = content.replace("'", '"')

    ifile.close()

    DataProc =  DataProcess()
    Viewer = DataViewer()

    try:
        json.loads(content)
    except ValueError:
        print("deserializations json file falied")
        return

    Json_List = DataProc.ImportJsonString(content)
    if Json_List is None:
        print("josn file load failed")
        return

    CALL_SATCK = FrameGraph("ns")

    count = len(Json_List)
    if index >= count:
        index = count -1
        pass

    jsonData = Json_List[index]
    if jsonData["call"]:
        Viewer.Handle_JsonData(CALL_SATCK, jsonData["call"])
        pass

    ofile = open('test.html', 'wb')
    ofile.write(CALL_SATCK.ToHTML().encode('utf-8'))
    ofile.close()

    print("Finish")

    pass

GenerateHtml("luarofiler.out", 50)