import sys  #,imp
import importlib
import urllib  # python3 allin urllib urllib2
import FrameGraph
# contents = urllib2.urlopen("http://gitlab.testplus.cn/snippets/14/raw").read()
# use python3 replace
# contents = urllib.request("http://gitlab.testplus.cn/snippets/14/raw").read()
#
# _mod_fg = imp.new_module('FrameGraph')
# exec contents in _mod_fg.__dict__
# FrameGraph, Matcher = _mod_fg.FrameGraph, _mod_fg.Matcher

FrameGraph, Matcher = FrameGraph.FrameGraph, FrameGraph.Matcher

CALL_SATCK = FrameGraph()
CALL_SATCK.Enter(u"layer0")
CALL_SATCK.Update(100)
CALL_SATCK.Leave()
CALL_SATCK.Enter(u"layer1")
CALL_SATCK.Update(100)
CALL_SATCK.Leave()
CALL_SATCK.Enter(u"layer2")
CALL_SATCK.Update(100)
CALL_SATCK.Leave()
# CALL_SATCK.Leave()
# CALL_SATCK.Leave()
CALL_SATCK.Enter(u"layer0.1")
CALL_SATCK.Enter(u"layer1.1")
CALL_SATCK.Update(10)
CALL_SATCK.Enter(u"layer2.1")
CALL_SATCK.Update(11)
CALL_SATCK.Enter(u"layer3")
CALL_SATCK.Update(12)
CALL_SATCK.Leave()
CALL_SATCK.Leave()
CALL_SATCK.Leave()
CALL_SATCK.Leave()

# CALL_SATCK = FrameGraph("ms")
# CALL_SATCK.Enter(u"layer1")
# CALL_SATCK.Alloc(1, 11)
# CALL_SATCK.Alloc(2, 22)
# CALL_SATCK.Enter(u"layer2")
# CALL_SATCK.Enter(u"layer2.1")
# CALL_SATCK.Alloc(3, 33)
# CALL_SATCK.Alloc(4, 44)
# CALL_SATCK.Leave()
# CALL_SATCK.Leave()
# # CALL_SATCK.Free(1)
# # CALL_SATCK.Free(3)
# CALL_SATCK.Leave()
# CALL_SATCK.Enter(u"layer1.1")
# CALL_SATCK.Alloc(5, 55)
# CALL_SATCK.Leave()
# CALL_SATCK.Enter(u"layer2")
# CALL_SATCK.Enter(u"layer3")
# CALL_SATCK.Alloc(7, 7)
# CALL_SATCK.Enter(u"layer4")
# CALL_SATCK.Alloc(6, 66)
# # CALL_SATCK.Leave()
# CALL_SATCK.Leave()
# CALL_SATCK.Leave()
# CALL_SATCK.Leave()

# print (CALL_SATCK.ToHTML().encode('utf-8'))
# CALL_SATCK.ToHTML().encode('utf8')

file = open('test.html','wb')
file.write(CALL_SATCK.ToHTML().encode('utf-8'))

file.close()

print("Finish")
# for i in CALL_SATCK._root._children.valu
# es():
#     walk(i, [])
# BOTTOM_UP.Print()



str=''''
{
    "t":	0.000601,
    "line":	225,
    "def":	499,
    "total":	0.004207,
    "count":	7,
    "ave":	0.000601,
    "mod":	"/Client/Profiling/KProfilingManager.lua",
    "func":	"EndSecondaryCheck"
}
'''
# print Matcher().match(str)
