//========= Copyright � 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "hud.h"
#include <VGUI_ActionSignal.h>
#include <VGUI_TextGrid.h>
#include <VGUI_TextEntry.h>
#include <VGUI_EtchedBorder.h>
#include <VGUI_LoweredBorder.h>
#include "vgui_XDMViewport.h"
#include "vgui_ConsolePanel.h"

using namespace vgui;

class ConsolePanelHandler : public ActionSignal
{
public:
	ConsolePanelHandler(ConsolePanel* consolePanel)
	{
		_consolePanel=consolePanel;
	}

	virtual void actionPerformed(Panel* panel)
	{
		_consolePanel->doExecCommand();
	}

private:
	ConsolePanel *_consolePanel;
};

// XDM: useless and obsolete stuff

ConsolePanel::ConsolePanel(int x,int y,int wide,int tall) : CMenuPanel(127, 0, x, y, wide, tall)// : Panel(x,y,wide,tall)
{
	setBorder(new EtchedBorder());

	_textGrid=new TextGrid(80,21,5,5,200,100);
	_textGrid->setBorder(new LoweredBorder());
	_textGrid->setParent(this);

	_textEntry=new TextEntry("",5,5,200,20);
	_textEntry->setParent(this);
	_textEntry->addActionSignal(new ConsolePanelHandler(this));
}

int ConsolePanel::print(const char* text)
{
	return _textGrid->printf("%s",text);
}

int ConsolePanel::vprintf(const char* format,va_list argList)
{
	return _textGrid->vprintf(format,argList);
}

int ConsolePanel::printf(const char* format,...)
{
	va_list argList;
	va_start(argList,format);
	int ret=vprintf(format,argList);
	va_end(argList);
	return ret;
}

void ConsolePanel::doExecCommand()
{
	char buf[2048];
	_textEntry->getText(0,buf,2048);
	_textEntry->setText(null,0);
	gEngfuncs.pfnClientCmd(buf);
}

void ConsolePanel::setSize(int wide,int tall)
{
	Panel::setSize(wide,tall);
		
	getPaintSize(wide,tall);

	_textGrid->setBounds(5,5,wide-10,tall-35);
	_textEntry->setBounds(5,tall-25,wide-10,20);
}
