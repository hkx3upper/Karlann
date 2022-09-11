
#include "libwsk.h"

ULONG  POOL_TAG = 'TSET'; // TEST

SOCKET ClientSocket = 0;
LPWSTR HostName = NULL;
LPWSTR PortName = NULL;
PADDRINFOEXW AddrInfo = NULL;


NTSTATUS 
StartWSKClientUDP(
    _In_opt_ LPCWSTR NodeName,
    _In_opt_ LPCWSTR ServiceName,
    _In_     ADDRESS_FAMILY AddressFamily,
    _In_     USHORT  SocketType
)
{
    NTSTATUS Status = STATUS_SUCCESS;

    ADDRINFOEXW Hints = { 0 };


    do
    {
        HostName = (LPWSTR)ExAllocatePoolZero(PagedPool, NI_MAXHOST * sizeof(WCHAR), POOL_TAG);
        PortName = (LPWSTR)ExAllocatePoolZero(PagedPool, NI_MAXSERV * sizeof(WCHAR), POOL_TAG);

        if (HostName == NULL || PortName == NULL)
        {
            PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
                ("%s->ExAllocatePoolZero Name failed.\n",
                    __FUNCTION__));

            Status = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }

        Hints.ai_family = AddressFamily;
        Hints.ai_socktype = SocketType;
        Hints.ai_protocol = ((SocketType == SOCK_DGRAM) ? IPPROTO_UDP : IPPROTO_TCP);

        Status = WSKGetAddrInfo(NodeName, ServiceName, NS_ALL, NULL,
            &Hints, &AddrInfo, WSK_INFINITE_WAIT, NULL, NULL);
        if (!NT_SUCCESS(Status))
        {
            PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
                ("%s->WSKGetAddrInfo failed. Status = 0x%x.\n",
                    __FUNCTION__, Status));

            break;
        }

        // Make sure we got at least one address back
        if (AddrInfo == NULL)
        {
            PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
                ("%s->Server (%ls) name could not be resolved.\n",
                    __FUNCTION__, NodeName));

            Status = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }

        for (PADDRINFOEXW Addr = AddrInfo; Addr; Addr = Addr->ai_next)
        {
            Status = WSKSocket(&ClientSocket, (ADDRESS_FAMILY)Addr->ai_family,
                (USHORT)Addr->ai_socktype, Addr->ai_protocol, NULL);
            if (!NT_SUCCESS(Status))
            {
                PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
                    ("%s->WSKSocket failed. Status = 0x%x.\n",
                        __FUNCTION__, Status));

                break;
            }

            Status = WSKGetNameInfo(Addr->ai_addr, (ULONG)Addr->ai_addrlen,
                HostName, NI_MAXHOST, PortName, NI_MAXSERV, NI_NUMERICHOST | NI_NUMERICSERV);
            if (!NT_SUCCESS(Status))
            {
                PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
                    ("%s->WSKGetNameInfo failed. Status = 0x%x.\n",
                        __FUNCTION__, Status));

                break;
            }

            PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
                ("%s->Client attempting connection to ip = %ls port = %ls.\n",
                    __FUNCTION__, HostName, PortName));

            if (Addr->ai_socktype == SOCK_DGRAM)
            {
                Status = WSKIoctl(ClientSocket, SIO_WSK_SET_SENDTO_ADDRESS,
                    Addr->ai_addr, Addr->ai_addrlen, NULL, 0, NULL, NULL, NULL);
            }

            if (NT_SUCCESS(Status))
            {
                break;
            }
        }

        if (!NT_SUCCESS(Status))
        {
            PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
                ("%s->Unable to establish connection. Status = 0x%x.\n",
                    __FUNCTION__, Status));

            break;
        }

        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->Connection established.\n",
                __FUNCTION__));
        

    } while (FALSE);

    return Status;
}


VOID 
CloseWSKClient(
)
{
    if (HostName)
    {
        ExFreePoolWithTag(HostName, POOL_TAG);
        HostName = NULL;
    }

    if (PortName)
    {
        ExFreePoolWithTag(PortName, POOL_TAG);
        PortName = NULL;
    }

    if (AddrInfo)
    {
        WSKFreeAddrInfo(AddrInfo);
        AddrInfo = NULL;
    }

    if (ClientSocket != 0)
    {
        WSKCloseSocket(ClientSocket);

        ClientSocket = 0;
    }
}
