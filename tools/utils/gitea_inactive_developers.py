# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Script to get all the inactive gitea developers
# Usage: GITEA_API_TOKEN=<yourtoken> python3 gitea_inactive_developers.py
#
# The API Token have the "read:org" or "admin:org" scope.
#
# Potential errors:
# * 403 Client Error: That means the token doesn't have the right scope.
# * 500 Server Error: The token is invalid.

import csv
import logging
import os
import requests
import sys
import yarl
from retry import retry as retry_decorator
import datetime
import dataclasses as dc
import iso8601

from typing import (
    cast,
    Callable,
    Dict,
    Iterable,
    List,
    NewType,
    Optional,
    Tuple,
    Type,
    TypeVar,
    Union,
)
from requests.structures import CaseInsensitiveDict

logger = logging.getLogger(__file__)


@dc.dataclass
class TeamMember():
    id: int
    login: str
    full_name: str
    last_login: datetime.datetime

    def __str__(self) -> str:
        return "{id};{login};{full_name};{last_login};{url}\n".format(
            id=self.id,
            login=self.login,
            full_name=self.full_name,
            last_login=self.last_login,
            url=gitea_domain + self.login,
        )


Page = NewType('Page', int)

F = TypeVar('F', bound=Callable[..., object])
T = TypeVar('T', bound=object)

retry: Callable[[F], F] = retry_decorator(
    tries=10, delay=1, backoff=2, logger=logger)


def assert_cast(typ: Type[T], obj: object) -> T:
    assert isinstance(obj, typ), f'object is not of type {typ}: {obj}'
    # NOTE: MYPY warns the cast is redundant, we might consider removing it.
    return cast(T, obj)  # type: ignore


def get_date_object(date_string: str) -> datetime.datetime:
    result = iso8601.parse_date(date_string)
    assert isinstance(result, datetime.datetime)
    return result


results_per_page = 25


def get_next_page(headers: CaseInsensitiveDict[str], page: Page) -> Optional[Page]:
    """
    Parse the header looking for reference to next.
    """
    total_count = int(assert_cast(str, headers.get('X-Total-Count')))

    # If current page already accounts to all the results we need
    # there is no need for extra pages.
    if page * results_per_page >= total_count:
        return None

    next_page = Page(page + 1)
    return next_page


@retry
def fetch_single(
    api_url: yarl.URL,
    api_token: str,
    method: str,
    data: Dict[str, str],
    page: Page,
) -> Tuple[List[object], Optional[Page]]:
    """Generic function to query a single item from the API.

    Returns:
        A dictionary containing the item data.
    """
    headers = {
        'accept': 'application/json',
        'Authorization': 'token ' + api_token,
    }

    params: Dict[str, Union[str, int]] = {
        'limit': results_per_page,
        'page': page,
        **data,
    }

    logger.info(f"Calling {method} ({params=}).")
    response = requests.get(str(api_url / method), params=params, headers=headers)
    response.raise_for_status()
    response_json = response.json()

    next_page = get_next_page(response.headers, page)
    return response_json, None if next_page is None else next_page


def fetch_all(
    api_url: yarl.URL,
    api_token: str,
    method: str,
    data: Dict[str, str],
) -> Iterable[object]:
    """Generic function to query lists from API.

    Yields:
        response_data - the result of fetch_single()
    """
    page = Page(1)
    while page is not None:
        response_data, page = fetch_single(api_url, api_token, method, data, page)
        yield from response_data if response_data is not None else ()


def fetch_team_members(
    api_url: yarl.URL,
    api_token: str,
    organization_name: str,
    team_name: str,
) -> List[TeamMember]:
    """Query API for all the members of a team.

    Yields:
        TeamMember objects.
    """

    method = "orgs/{org}/teams".format(org=organization_name)
    team_id = None

    for team in cast(
        Iterable[Dict[object, object]],
        fetch_all(
            api_url,
            api_token,
            method,
            data={},
        ),
    ):
        if team.get('name') != team_name:
            continue

        team_id = team.get('id')
        break

    if team_id is None:
        logger.error('No team found with name: ' + team_name)
        sys.exit(2)

    method = "teams/{id}/members".format(id=team_id)
    users = list()

    for member in cast(
        Iterable[Dict[object, object]],
        fetch_all(
            api_url,
            api_token,
            method,
            data={},
        ),
    ):
        users.append(
            TeamMember(
                id=assert_cast(int, member.get('id')),
                login=assert_cast(str, member.get('login')),
                full_name=assert_cast(str, member.get('full_name')),
                last_login=get_date_object(
                    assert_cast(str, member.get('last_login'))),
            ))

    return users


def is_inactive(member: TeamMember) -> bool:
    """
    Returns whether the member is no longer active.

    Users are active when they logged in the past 2 years.
    """
    tzinfo = member.last_login.tzinfo
    two_years_ago = datetime.datetime.now(tzinfo) - datetime.timedelta(days=2 * 365)
    return member.last_login < two_years_ago


teams = (
    "Developers",
    "Add-ons",
    "Translation",
    "Documentation",
    "Technical-Artists",
    "Contributors",
)


api_token = os.environ['GITEA_API_TOKEN']
gitea_domain = "https://projects.blender.org/"
api_url = yarl.URL(gitea_domain + 'api/v1/')
organization_name = "blender"


def main() -> None:
    for team_name in teams:
        logger.warning(team_name)
        members = fetch_team_members(
            api_url, api_token, organization_name, team_name)

        inactive_members = [str(m) for m in members if is_inactive(m)]

        logger.warning("  Total members:" + str(len(members)))
        logger.warning("  Inactive members: " + str(len(inactive_members)))

        file_name = team_name.lower() + ".csv"
        with open(file_name, 'w', newline='') as csv_file:
            csv_file.write("id;login;full_name;last_login;url\n")
            csv_file.writelines(inactive_members)
        logger.warning("  Output file: " + file_name)


if __name__ == "__main__":
    main()
