
function emit_doc_start()
print[[
\documentclass[singleside,english, a4paper]{article}
\usepackage{graphicx}
\usepackage{amssymb,amsmath}
\usepackage{color}

\listfiles

\title{Work Breakdown Document\\\small \textsc{OpenDNSSEC Enforcer}  }
\author{Ren\'{e} Post, rene@xpt.nl}
\date{\today}

\setcounter{tocdepth}{3}

\begin{document}
\maketitle
\tableofcontents
]]
end

function emit_doc_end()
print[[
\end{document}
]]
end

local state = { description = "", responsibilities = {}, requires = {}, work = {}, graphic = {} }
local g_state = { work = {} }

function emit_work_overview()
	totalhours= 0
	totalconfidence = 0.0
	runcount = 0
	if #g_state.work ~= 0 then
		print([[\section{Work Overview}]])
		print[[\begin{tabular}{ l | p{7cm} | l | l }]]
		print[[ Target & Description & Hours & Confidence \\]]
		print[[\hline]]
		for _,v in ipairs(g_state.work) do
			print(v.target .. " & " .. v.description .. " & " .. v.hours .. " & " .. v.confidence .. [[ \\]])
			
			totalhours = totalhours + v.hours;
			totalconfidence = totalconfidence + (v.hours * v.confidence)
			
			runcount=runcount + 1
			if (runcount % 33) == 0 then
				print[[\end{tabular}]]
				print[[\clearpage]]
				print[[\begin{tabular}{ l | p{7cm} | l | l  }]]
				print[[ Target & Description & Hours & Confidence \\]]
				print[[\hline]]
			end
		end
		print[[\hline]]
		print(" & total & " .. totalhours .. " & " .. string.format("%.2g",(totalconfidence / totalhours)) .. [[ \\]])
		print[[\end{tabular}]]
	end
end

function target(t)
	print[[\clearpage]]
	print([[\section{]] .. t.name .. [[}]])

	print[[\begin{tabular}{l|l|l}]]
	print[[Kind & Language & Origin \\ \hline]]
	print( t.kind .. " & " .. t.language .. " & " .. t.origin )
	print[[\end{tabular}]]


	print([[\subsection{Description}]])
	print(state.description)
	
	if #state.responsibilities ~= 0 then
		print([[\subsection{Responsibilities}]])
	end
	for _,v in ipairs(state.responsibilities) do
		print([[\subsubsection{]] .. v.name .. [[}]])
		for _,w in ipairs(v) do
			print(w)
		end
	end
	
	for _,k in ipairs(state.graphic) do
		print[=[\begin{figure}[h]]=]
		print[=[\centering]=]
		print([=[\includegraphics[scale=0.30]{]=] .. k.filepath .. [=[}]=])
		if k.caption then
			print([=[\caption{]=] .. k.caption .. [=[}]=])
		end
		if k.refid then
			print([=[\label{]=].. k.refid .. [=[}]=])
		end
		print[=[\end{figure}]=]
	end

	if #state.requires ~= 0 then
		print([[\subsection{Requires}]])
		print(table.concat(state.requires,[[ \\ ]]))
	end

	if #state.work ~= 0 then
		print([[\subsection{Work}]])
		print[[\begin{tabular}{ l | l | p{9cm}  }]]
		print[[ Hours & Confidence & Description \\]]
		print[[\hline]]
		for _,v in ipairs(state.work) do
			print((v.hours or "0") .. " & " .. (v.confidence or "0.0") .. " & " .. v.description .. [[ \\]])
			table.insert(g_state.work,{ target=t.name, hours=v.hours, confidence=v.confidence, description=v.description })
		end
		print[[\end{tabular}]]
	end

	state = { description = "", responsibilities = {}, requires = {}, work = {}, graphic = {} }
end

function description(s)
	state.description = s
end

function responsibility(t)
	table.insert(state.responsibilities,t)
end

function requires(t)
	for _,v in ipairs(t) do
		table.insert(state.requires,v)
	end
end

function work(t)
end

function graphic(t)
	table.insert(state.graphic,t)
end

function task(t)
	if #t > 0 then
		table.insert(state.work,{ hours=t.hours, confidence=t.confidence, description=t[1]})
	end
end

function dependency(t)
--[[
	print("dependency " .. t.name)
	for _,v in ipairs(state.responsibilities) do
		print("\t" .. v.name)
		for _,w in ipairs(v) do
			print("\t\t" .. w)
		end
	end
	state.responsibilities = {}
--]]
end

function test(t)
--	print("test " .. t.name)
end

function scenario(t)
--	print("scenario " .. t.name)
end


--emit_doc_start();dofile(arg[1]);emit_work_overview();emit_doc_end()
