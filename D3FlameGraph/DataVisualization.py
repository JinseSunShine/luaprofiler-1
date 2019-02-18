import sys , os
import json
import FrameGraph
import math
from tkinter import messagebox
FrameGraph, Matcher = FrameGraph.FrameGraph, FrameGraph.Matcher

def get_node_unique_info(node):
    strInfo = node["mod"] + node["func"]+str(node["def"]) + node["cf"]+ str(node["cl"])
    return strInfo

class DataProcess:

    def __init__(self):
        self._json_dic = {}
        self._jsondata = None
        self._nodeCount = 0
        self._statistic_info = {}

        pass

    def ImportJsonFile(self, file):
        jsonData = json.load(file)
        self._jsondata = jsonData
        self._nodeCount = len(jsonData)
        return jsonData
        pass

    def ImportJsonString(self, content):
        try:
            jsonData = json.loads(content)
            self._jsondata = jsonData
            self._nodeCount = len(jsonData)
            return jsonData
        except ValueError:
            print("json file format error")
            self._jsondata = None
            return None
        pass

    def isAvalible(self):
        return self._jsondata != None

    def getDataNode(self, index):
        return self._jsondata[index]

    def getCount(self):
        return self._nodeCount

    def statisticCalledInfo(self, node):
        if not node:
            return
        # same function & same call line
        str_funcInfo = get_node_unique_info(node)

        if not self._statistic_info.get(str_funcInfo):
            self._statistic_info[str_funcInfo] = {
                "t" : node["t"],
                "count" : 1,
            }
        else:
            self._statistic_info[str_funcInfo]["t"] += node["t"]
            self._statistic_info[str_funcInfo]["count"] += 1

        if "sub" in node:
            for subnode in node["sub"]:
                self.statisticCalledInfo(subnode)

        pass

    #
    def sampling_statistics(self, startIndex, endIndex):
        if endIndex < startIndex :
            print("sample_and_merge_data failed endIndex < startIndex")
            return

        self.ClearStatisticInfo()

        for index in range(startIndex, endIndex):
            node = self.getDataNode(index)
            self.statisticCalledInfo(node)
        pass

    def get_func_statisticInfo(self, str_funcInfo):
        return self._statistic_info.get(str_funcInfo)

    def ClearStatisticInfo(self):
        self._statistic_info.clear()

    def Clear(self):
        self.ClearStatisticInfo()
        pass

class DataViewer:
    def __init__(self,filepath=None):
        #
        self._DataProcess = DataProcess()
        self._filepath = None
        self._data_handle_flag = {}

        if filepath != None:
            self.OpneJsonFile(filepath)
        pass

    def OpneJsonFile(self, filepath, split_time = 1):

        if not os.path.exists(filepath):
            print("input file path is not exists:"+filepath)
            return False

        ifile = open(filepath, 'r', encoding='utf-8')
        content = ifile.read()
        content = content.replace("'", '"')
        ifile.close()
        _jsondata = self._DataProcess.ImportJsonString(content)
        if _jsondata is None:
            print("josn file load failed")
            return False

        self._filepath = filepath
        self._splitTime = split_time
        self._recordInfo = {
            "sampleIndex" : 0,
            "presampleIndex" : 0,
            "endsampleIndex" :0,
        }

        return True

    def get_sample_node_count(self):
        nodecount = self._DataProcess.getCount()
        if nodecount <= 0:
            messagebox.showinfo("info","nodecount is 0")
            return 0
        node = self._DataProcess.getDataNode(nodecount -1)  # total info

        split_count = math.ceil(node["total_cs"] / (self._splitTime *60))
        if split_count <= 0:
            split_count = 1
        return split_count

    def Handle_JsonData(self, call_satck, root):
        if type(root) != list:
            return

        length = len(root)
        if length <= 0 :
            return

        index = 0
        while index < length:
            call_info = root[index]
            str_funcInfo = get_node_unique_info(call_info)

            # if self._data_handle_flag.get(str_funcInfo) != None:
            #     index += 1
            #     continue
            # self._data_handle_flag[str_funcInfo] = 1

            strdesc = call_info["mod"]+" fun:"+call_info["func"]+u" line:"+str(call_info["def"])+" @"+str(call_info["cf"])+str(call_info["cl"])
            call_satck.Enter( strdesc)
            cost_time = call_info["t"]
            if self._DataProcess.get_func_statisticInfo(str_funcInfo) != None:
                cost_time = self._DataProcess.get_func_statisticInfo(str_funcInfo)["t"]

            call_satck.Update(cost_time*1000)
            if "sub" in call_info:
                self.Handle_JsonData(call_satck, call_info["sub"])
                pass

            call_satck.Leave()
            index += 1
        pass



    def get_sample_data(self, is_next):
        sample_data = None
        if is_next:
            self._recordInfo["presampleIndex"] = self._recordInfo["sampleIndex"]
            self._recordInfo["sampleIndex"] = self._recordInfo["endsampleIndex"]

            node = self._DataProcess.getDataNode(self._recordInfo["sampleIndex"])
            record_time = node["n"] or 0
            count = self._DataProcess.getCount()

            if self._recordInfo["sampleIndex"] >= (count-2):
                return None

            interval = 0
            is_find = False
            sample_data = []
            for i in range(self._recordInfo["sampleIndex"], count-2):
                node = self._DataProcess.getDataNode(i)
                interval += node["n"] - record_time
                record_time = node["n"]

                sample_data.append(node)
                if interval >= self._splitTime:
                    self._recordInfo["endsampleIndex"] = i
                    is_find = True
                    break
            if not is_find :
                self._recordInfo["endsampleIndex"] = count-2
        else:
            # TODO :
            pass

        self._DataProcess.sampling_statistics(self._recordInfo["sampleIndex"], self._recordInfo["endsampleIndex"])
        return sample_data

    def FlushData(self, is_next = True):

        if not self._DataProcess.isAvalible():
            return None, -1

        root_node = self.get_sample_data(is_next)
        if root_node == None:
            return None, -2

        self._data_handle_flag.clear()
        try:
            frameGraph = FrameGraph("us")
            self.Handle_JsonData(frameGraph, root_node)

            return frameGraph, 0

        except ValueError:
            exit(5)
        pass

    def GetTotalCostInfo(self):
        record_info = {}
        node_count = self._DataProcess.getCount()
        for i in range(0, node_count - 1):
            node = self._DataProcess.getDataNode(i)
            self.record_totalinfo(record_info, node)

        lst_info = sorted(record_info.items(), key=lambda item: item[1], reverse=True)

        jsObj = json.dumps(lst_info, indent =1)

        record_info.clear()
        return jsObj

    def record_totalinfo(self, record_dict, node):
        func_def = node["func"] + ' @' + node["mod"] + '-line:' + str(node["def"])
        if not record_dict.get(func_def):
            record_dict[func_def] = node["t"]
        else:
            record_dict[func_def] += node['t']

        if "sub" in node:
            for subnode in node["sub"]:
                self.record_totalinfo(record_dict,subnode)

    def Clear(self):
        self._DataProcess.Clear()
        self._recordInfo.clear()
        pass


def main(argv):
    import time

    # DataViewer init
    _mDataViewer = DataViewer()

    # import json file
    file_path = argv[1]
    split_time  = 1
    if len(argv) > 2 and  argv[2]:
        split_time = float(argv[2])

    if _mDataViewer.OpneJsonFile(file_path, split_time):
        splits_count = _mDataViewer.get_sample_node_count()
        if splits_count <= 0:
            print(u"Not contain avaliable value data")
            return

        path = os.path.realpath(sys.argv[0])
        path = os.path.split(path)[0]
        dir_name = path + '/' + time.strftime("%Y-%m-%d-%H-%M-%S", time.localtime())

        if not os.path.exists(dir_name):
            os.mkdir(dir_name)

        bSuccess = False
        for i in range(0, splits_count):
            print("Process Splite :"+ str(i))

            frameGraph, code = _mDataViewer.FlushData(True)
            if frameGraph == None:
                print("FlushData failed not analysis data" + str(code))
                break

            ofile = open('{0}/example_{1}.html'.format(dir_name, i), 'wb')
            ofile.write(frameGraph.ToHTML().encode('utf-8'))
            ofile.close()

            if i == (splits_count - 1):
                bSuccess = True
            pass

        # export total cost info
        jsData = _mDataViewer.GetTotalCostInfo()
        ofile = open('{0}/totalinfo.json'.format(dir_name), 'w')
        ofile.write(jsData)
        ofile.close()

        if bSuccess :
            import webbrowser
            print("Process Success!!")
            # Open Web viewer
            url = str.format('{0}/example_0.html',dir_name)
            print(url)
            webbrowser.open(url, 0, False)
        else:
            print("Process Failed ,please check it !!")
    pass

if __name__ == '__main__':
    main(sys.argv)
